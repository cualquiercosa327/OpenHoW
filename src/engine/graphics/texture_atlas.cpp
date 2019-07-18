/* OpenHoW
 * Copyright (C) 2017-2019 Mark Sowden <markelswo@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../engine.h"

#include "display.h"
#include "texture_atlas.h"

TextureAtlas::TextureAtlas() = default;
TextureAtlas::~TextureAtlas() {
  for(auto& id : images_by_name_) {
    PLImage *image = id.second;
    plDestroyImage(image);
    id.second = nullptr;
  }

  plDestroyTexture(texture_, true);
}

void TextureAtlas::AddImage(const std::string &path) {
  const auto image = images_by_name_.find(path);
  if(image != images_by_name_.end()) {
    return;
  }

  char full_path[PL_SYSTEM_MAX_PATH];
  snprintf(full_path, sizeof(full_path) - 1, "%s", u_find2(path.c_str(), supported_image_formats, true));
  auto* img = static_cast<PLImage *>(u_alloc(1, sizeof(PLImage), true));
  if(!plLoadImage(full_path, img)) {
    Error("Failed to load image (%s)!\n", plGetError());
  }

  plConvertPixelFormat(img, PL_IMAGEFORMAT_RGBA8);
  images_by_name_.emplace(path, img);
  images_by_height_.emplace(img->height, img);
}

void TextureAtlas::AddImages(const std::vector<std::string> &textures) {
  for(const auto &path : textures) {
    AddImage(path);
  }
}

void TextureAtlas::Finalize() {
    // Figure out how we'll organise the atlas
  unsigned int w = 512, h = 8;
  unsigned int max_h = 0;
  unsigned int cur_y = 0, cur_x = 0;
  for(auto i = images_by_height_.rbegin(); i != images_by_height_.rend(); ++i) {
    PLImage *image = i->second;
    if(image->height > max_h) {
      max_h = image->height;
    }

    if(cur_x == 0 && image->width > w) {
      w = image->width;
    } else if(cur_x + image->width > w) {
      cur_y += max_h;
      cur_x = max_h = 0;
    }

    if(cur_y + image->height > h) {
      h = cur_y + image->height;
    }

    u_assert(image->path[0] != '\0', "Invalid image name!");
    const char *filename = plGetFileName(image->path);
    const char *extension = plGetFileExtension(image->path);
    std::string index_name = std::string(filename).substr(0, strlen(filename) - (strlen(extension) + 1));
    textures_.emplace(index_name, Index {
        .x = cur_x,
        .y = cur_y,
        .w = image->width,
        .h = image->height,
        .image = image
    });

    cur_x += image->width;
  }

  // Ensure power of two for final atlas
  h = static_cast<unsigned int>(pow(2, ceil(log(h) / log(2))));

  // Image pointers will be freed when we're done with textures list
  images_by_name_.clear();
  images_by_height_.clear();

  // Now create the atlas itself
  PLImage* cache = plCreateImage(nullptr, w, h, PL_COLOURFORMAT_RGBA, PL_IMAGEFORMAT_RGBA8);
  if(cache == nullptr) {
    Error("Failed to generate image cache for texture atlas (%s)!\n", plGetError());
  }

  // todo: generate mipmap levels
  cache->data = (uint8_t**)u_alloc(cache->levels, sizeof(uint8_t *), true);
  cache->data[0] = (uint8_t*)u_alloc(cache->size, sizeof(uint8_t), true);

  for(auto& tarr : textures_) {
    Index *texture = &tarr.second;
    uint8_t* pos = cache->data[0] + ((texture->y * cache->width) + texture->x) * 4;
    uint8_t* src = texture->image->data[0];
    for(unsigned int y = 0; y < texture->h; ++y) {
      memcpy(pos, src, (texture->w * 4));
      src += texture->w * 4;
      pos += cache->width * 4;
    }

    plFreeImage(texture->image);
    texture->image = nullptr;
  }

#if 0
  {
    static unsigned int gen_id = 0;
    if(plCreatePath("./debug/atlas_data/")) {
      char buf[PL_SYSTEM_MAX_PATH];
      snprintf(buf, sizeof(buf) - 1, "./debug/atlas_data/%dx%d_%d.png",
               cache->width, cache->height, gen_id++);
      plWriteImage(cache, buf);
    }
  }
#endif

  if((texture_ = plCreateTexture()) == nullptr) {
    Error("Failed to generate atlas texture (%s)!\n", plGetError());
  }

  texture_->filter = PL_TEXTURE_FILTER_NEAREST;
  if(!plUploadTextureImage(texture_, cache)) {
    Error("Failed to upload texture atlas (%s)!\n", plGetError());
  }

  plFreeImage(cache);
}

void TextureAtlas::GetTextureCoords(const std::string &name, float *x, float *y, float *w, float *h) {
  auto index = textures_.find(name);
  if(index == textures_.end()) {
    *x = *y = 0;
    *w = *h = 1.0f;
    return;
  }

  *x = static_cast<float>(index->second.x) / static_cast<float>(texture_->w);
  *y = static_cast<float>(index->second.y) / static_cast<float>(texture_->h);
  *w = static_cast<float>(index->second.w) / static_cast<float>(texture_->w);
  *h = static_cast<float>(index->second.h) / static_cast<float>(texture_->h);
}
