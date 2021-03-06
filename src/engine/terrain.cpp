/* OpenHoW
 * Copyright (C) 2017-2020 Mark Sowden <markelswo@gmail.com>
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

#include <list>

#include "engine.h"
#include "terrain.h"

#include "graphics/mesh.h"
#include "graphics/shaders.h"
#include "graphics/texture_atlas.h"
#include "graphics/display.h"

//Precalculated vertices for chunk rendering
//TODO: Share one index buffer instance between all chunks
const static unsigned int chunk_indices[96] = {
	0, 2, 1, 1, 2, 3,
	4, 6, 5, 5, 6, 7,
	8, 10, 9, 9, 10, 11,
	12, 14, 13, 13, 14, 15,
	16, 18, 17, 17, 18, 19,
	20, 22, 21, 21, 22, 23,
	24, 26, 25, 25, 26, 27,
	28, 30, 29, 29, 30, 31,
	32, 34, 33, 33, 34, 35,
	36, 38, 37, 37, 38, 39,
	40, 42, 41, 41, 42, 43,
	44, 46, 45, 45, 46, 47,
	48, 50, 49, 49, 50, 51,
	52, 54, 53, 53, 54, 55,
	56, 58, 57, 57, 58, 59,
	60, 62, 61, 61, 62, 63,
};

Terrain::Terrain( const std::string& tileset ) {
	// attempt to load in the atlas sheet
	// TODO: allow us to change this on the fly
	atlas_ = new TextureAtlas( 512, 8 );
	for ( unsigned int i = 0; i < 256; ++i ) {
		if ( !atlas_->AddImage( tileset + std::to_string( i ) ) ) {
			break;
		}
	}
	atlas_->Finalize();

	chunks_.resize( TERRAIN_CHUNKS );

	Update();
}

Terrain::~Terrain() {
	delete atlas_;

	for ( auto& chunk : chunks_ ) {
		plDestroyModel( chunk.model );
	}
}

Terrain::Chunk* Terrain::GetChunk( const PLVector2& pos ) {
	if ( pos.x < 0 || std::floor( pos.x ) >= TERRAIN_PIXEL_WIDTH ||
		pos.y < 0 || std::floor( pos.y ) >= TERRAIN_PIXEL_WIDTH ) {
		return nullptr;
	}

	uint idx =
		( ( uint ) ( pos.x ) / TERRAIN_CHUNK_PIXEL_WIDTH )
			+ ( ( ( uint ) ( pos.y ) / TERRAIN_CHUNK_PIXEL_WIDTH ) * TERRAIN_CHUNK_ROW );
	if ( idx >= chunks_.size() ) {
		LogWarn( "Attempted to get an out of bounds chunk index (%d)!\n", idx );
		return nullptr;
	}

	return &chunks_[ idx ];
}

Terrain::Tile* Terrain::GetTile( const PLVector2& pos ) {
	Chunk* chunk = GetChunk( pos );
	if ( chunk == nullptr ) {
		return nullptr;
	}

	uint idx = ( ( ( uint ) ( pos.x ) / TERRAIN_TILE_PIXEL_WIDTH ) % TERRAIN_CHUNK_ROW_TILES ) +
		( ( ( ( uint ) ( pos.y ) / TERRAIN_TILE_PIXEL_WIDTH ) % TERRAIN_CHUNK_ROW_TILES ) * TERRAIN_CHUNK_ROW_TILES );
	if ( idx >= TERRAIN_CHUNK_TILES ) {
		LogWarn( "Attempted to get an out of bounds tile index!\n" );
		return nullptr;
	}

	return &chunk->tiles[ idx ];
}

float Terrain::GetHeight( const PLVector2& pos ) {
	Tile* tile = GetTile( pos );
	if ( tile == nullptr ) {
		return 0;
	}

	float tile_x = pos.x - std::floor( pos.x );
	float tile_y = pos.y - std::floor( pos.y );

	float x = tile->height[ 0 ] + ( ( tile->height[ 1 ] - tile->height[ 0 ] ) * tile_x );
	float y = tile->height[ 2 ] + ( ( tile->height[ 3 ] - tile->height[ 2 ] ) * tile_x );
	float z = x + ( ( y - x ) * tile_y );

	return z;
}

void Terrain::GenerateModel( Chunk* chunk, const PLVector2& offset ) {
	if ( chunk->model != nullptr ) {
		plDestroyModel( chunk->model );
		chunk->model = nullptr;
	}

	PLMesh
		* chunk_mesh = plCreateMeshInit( PL_MESH_TRIANGLES, PL_DRAW_DYNAMIC, 32, 64, ( void* ) chunk_indices, nullptr );
	if ( chunk_mesh == nullptr ) {
		Error( "Unable to create map chunk mesh, aborting (%s)!\n", plGetError() );
	}

	int cm_idx = 0;
	for ( unsigned int tile_y = 0; tile_y < TERRAIN_CHUNK_ROW_TILES; ++tile_y ) {
		for ( unsigned int tile_x = 0; tile_x < TERRAIN_CHUNK_ROW_TILES; ++tile_x ) {
			const Tile* current_tile = &chunk->tiles[ tile_x + tile_y * TERRAIN_CHUNK_ROW_TILES ];

			float tx_x, tx_y, tx_w, tx_h;
			atlas_->GetTextureCoords( std::to_string( current_tile->texture ), &tx_x, &tx_y, &tx_w, &tx_h );

			// TERRAIN_FLIP_FLAG_X flips around texture sheet coords, not TERRAIN coords.
			if ( current_tile->rotation & Tile::ROTATION_FLAG_X ) {
				tx_x = tx_x + tx_w;
				tx_w = -tx_w;
			}

			// ST coords for each corner of the tile.
			float tx_Ax[] = { tx_x, tx_x + tx_w, tx_x, tx_x + tx_w };
			float tx_Ay[] = { tx_y, tx_y, tx_y + tx_h, tx_y + tx_h };

			// Rotate a quad of ST coords 90 degrees clockwise.
			auto rot90 = []( float* x ) {
				float c = x[ 0 ];
				x[ 0 ] = x[ 2 ];
				x[ 2 ] = x[ 3 ];
				x[ 3 ] = x[ 1 ];
				x[ 1 ] = c;
			};

			if ( current_tile->rotation & Tile::ROTATION_FLAG_ROTATE_90 ) {
				rot90( tx_Ax );
				rot90( tx_Ay );
			}

			if ( current_tile->rotation & Tile::ROTATION_FLAG_ROTATE_180 ) {
				rot90( tx_Ax );
				rot90( tx_Ay );
				rot90( tx_Ax );
				rot90( tx_Ay );
			}

			// MAP_FLIP_FLAG_ROTATE_270 is implemented by ORing 90 and 180 together.

			for ( int i = 0; i < 4; ++i, ++cm_idx ) {
				float x = ( offset.x * TERRAIN_CHUNK_PIXEL_WIDTH ) + ( tile_x + ( i % 2 ) ) * TERRAIN_TILE_PIXEL_WIDTH;
				float z = ( offset.y * TERRAIN_CHUNK_PIXEL_WIDTH ) + ( tile_y + ( i / 2 ) ) * TERRAIN_TILE_PIXEL_WIDTH;
				plSetMeshVertexST( chunk_mesh, cm_idx, tx_Ax[ i ], tx_Ay[ i ] );
				plSetMeshVertexPosition( chunk_mesh, cm_idx, { x, current_tile->height[ i ], z } );
				plSetMeshVertexColour( chunk_mesh, cm_idx, {
					current_tile->shading[ i ],
					current_tile->shading[ i ],
					current_tile->shading[ i ] } );
			}
		}
	}

	chunk_mesh->texture = atlas_->GetTexture();

	// attach the mesh to our model
	PLModel* model = plCreateBasicStaticModel( chunk_mesh );
	if ( model == nullptr ) {
		Error( "Failed to create map model (%s), aborting!\n", plGetError() );
	}

	chunk->model = model;
}

void Terrain::GenerateOverview() {
	static const PLColour colours[] = {
		{ 60, 50, 40 },     // Mud
		{ 40, 70, 40 },     // Grass
		{ 128, 128, 128 },  // Metal
		{ 153, 94, 34 },    // Wood
		{ 90, 90, 150 },    // Water
		{ 50, 50, 50 },     // Stone
		{ 50, 50, 50 },     // Rock
		{ 100, 80, 30 },    // Sand
		{ 180, 240, 240 },  // Ice
		{ 100, 100, 100 },  // Snow
		{ 60, 50, 40 },     // Quagmire
		{ 100, 240, 53 }    // Lava/Poison
	};

	// Create our storage
	PLImage* image = plCreateImage( nullptr, 64, 64, PL_COLOURFORMAT_RGB, PL_IMAGEFORMAT_RGB8 );

	// Now write into the image buffer
	uint8_t* buf = image->data[ 0 ];
	for ( uint8_t y = 0; y < 64; ++y ) {
		for ( uint8_t x = 0; x < 64; ++x ) {
			PLVector2 position( x * ( TERRAIN_PIXEL_WIDTH / 64 ), y * ( TERRAIN_PIXEL_WIDTH / 64 ) );
			Tile* tile = GetTile( position );
			u_assert( tile != nullptr, "Hit an invalid tile during overview generation!\n" );

			auto mod = static_cast<int>(( GetHeight( position ) + ( ( GetMaxHeight() + GetMinHeight() ) / 2 ) ) / 255);
			PLColour rgb = PLColour(
				std::min( ( colours[ tile->surface ].r / 9 ) * mod, 255 ),
				std::min( ( colours[ tile->surface ].g / 9 ) * mod, 255 ),
				std::min( ( colours[ tile->surface ].b / 9 ) * mod, 255 )
			);
			if ( tile->behaviour & Tile::BEHAVIOUR_MINE ) {
				rgb = PLColour( 255, 0, 0 );
			}

			*( buf++ ) = rgb.r;
			*( buf++ ) = rgb.g;
			*( buf++ ) = rgb.b;
		}
	}

#ifdef _DEBUG
	if ( plCreatePath( "./debug/generated/" ) ) {
		char path[PL_SYSTEM_MAX_PATH];
		static unsigned int id = 0;
		snprintf( path, sizeof( path ) - 1, "./debug/generated/%dx%d_%d.png",
				  image->width, image->height, id );
		plWriteImage( image, path );
	}
#endif

	// Allow rebuilding overview texture
	plDestroyTexture( overview_ );

	if ( ( overview_ = plCreateTexture() ) == nullptr ) {
		Error( "Failed to generate overview texture slot!\n%s\n", plGetError() );
	}

	plUploadTextureImage( overview_, image );
	plDestroyImage( image );
}

void Terrain::Update() {
	GenerateOverview();

	for ( unsigned int chunk_y = 0; chunk_y < TERRAIN_CHUNK_ROW; ++chunk_y ) {
		for ( unsigned int chunk_x = 0; chunk_x < TERRAIN_CHUNK_ROW; ++chunk_x ) {
			GenerateModel( &chunks_[ chunk_x + chunk_y * TERRAIN_CHUNK_ROW ],
						   { static_cast<float>(chunk_x), static_cast<float>(chunk_y) } );
		}
	}

	std::list<PLMesh*> meshes;
	for ( auto& chunk : chunks_ ) {
		PLModelLod* lod = plGetModelLodLevel( chunk.model, 0 );
		meshes.push_back( lod->meshes[ 0 ] );
	}

	Mesh_GenerateFragmentedMeshNormals( meshes );
}

void Terrain::Draw() {
	Shaders_SetProgramByName( cv_graphics_debug_normals->b_value ? "debug_normals" : "generic_textured_lit" );

	g_state.gfx.num_chunks_drawn = 0;
	for ( const auto& chunk : chunks_ ) {
		if ( chunk.model == nullptr ) {
			continue;
		}

		g_state.gfx.num_chunks_drawn++;
		plDrawModel( chunk.model );
	}
}

void Terrain::LoadPmg( const std::string& path ) {
	PLFile* fh = plOpenFile( path.c_str(), false );
	if ( fh == nullptr ) {
		LogWarn( "Failed to open tile data, \"%s\", aborting\n", path.c_str() );
		return;
	}

	for ( unsigned int chunk_y = 0; chunk_y < TERRAIN_CHUNK_ROW; ++chunk_y ) {
		for ( unsigned int chunk_x = 0; chunk_x < TERRAIN_CHUNK_ROW; ++chunk_x ) {
			Chunk& current_chunk = chunks_[ chunk_x + chunk_y * TERRAIN_CHUNK_ROW ];

			struct __attribute__((packed)) {
				/* offsets */
				uint16_t x{ 0 };
				uint16_t y{ 0 };
				uint16_t z{ 0 };
				uint16_t unknown0{ 0 };
			} chunk;

			bool status;
			chunk.x = plReadInt16( fh, false, &status );
			chunk.y = plReadInt16( fh, false, &status );
			chunk.z = plReadInt16( fh, false, &status );
			chunk.unknown0 = plReadInt16( fh, false, &status );

			if ( !status ) {
				Error( "Failed to read in chunk descriptor in \"%s\"!\n", plGetFilePath( fh ) );
			}

			struct __attribute__((packed)) {
				int16_t height{ 0 };
				uint16_t lighting{ 0 };
			} vertices[25];

			// Find the maximum and minimum points
			for ( auto& vertex : vertices ) {
				vertex.height = plReadInt16( fh, false, &status );
				vertex.lighting = plReadInt16( fh, false, &status );

				if ( !status ) {
					Error( "Failed to read in vertex descriptor in \"%s\"!\n", plGetFilePath( fh ) );
				}

				if ( static_cast<float>(vertex.height) > max_height_ ) {
					max_height_ = vertex.height;
				}
				if ( static_cast<float>(vertex.height) < min_height_ ) {
					min_height_ = vertex.height;
				}
			}

			plFileSeek( fh, 4, PL_SEEK_CUR );

			for ( unsigned int tile_y = 0; tile_y < TERRAIN_CHUNK_ROW_TILES; ++tile_y ) {
				for ( unsigned int tile_x = 0; tile_x < TERRAIN_CHUNK_ROW_TILES; ++tile_x ) {
					struct __attribute__((packed)) {
						int8_t unused0[6]{ 0, 0, 0, 0, 0, 0 };
						uint8_t type{ 0 };
						uint8_t slip{ 0 };
						int16_t unused1{ 0 };
						uint8_t rotation{ 0 };
						uint32_t texture{ 0 };
						uint8_t unused2{ 0 };
					} tile;

					// Skip unused data
					if ( plReadFile( fh, tile.unused0, 6, 1 ) != 1 ) {
						Error( "Failed to skip unused bytes in \"%s\"!\n", plGetFilePath( fh ) );
					}

					tile.type = plReadInt8( fh, &status );
					tile.slip = plReadInt8( fh, &status );
					tile.unused1 = plReadInt16( fh, false, &status );
					tile.rotation = plReadInt8( fh, &status );
					tile.texture = plReadInt32( fh, false, &status );
					tile.unused2 = plReadInt8( fh, &status );

					if ( !status ) {
						Error( "Failed to read in tile descriptor in \"%s\"!\n", plGetFilePath( fh ) );
					}

					Tile* current_tile = &current_chunk.tiles[ tile_x + tile_y * TERRAIN_CHUNK_ROW_TILES ];
					current_tile->surface = static_cast<Tile::Surface>(tile.type & 31U);
					current_tile->behaviour = static_cast<Tile::Behaviour>(tile.type & ~31U);
					current_tile->rotation = static_cast<Tile::Rotation>(tile.rotation);
					current_tile->slip = 0;
					current_tile->texture = tile.texture;

					current_tile->height[ 0 ] = vertices[ ( tile_y * 5 ) + tile_x ].height;
					current_tile->height[ 1 ] = vertices[ ( tile_y * 5 ) + tile_x + 1 ].height;
					current_tile->height[ 2 ] = vertices[ ( ( tile_y + 1 ) * 5 ) + tile_x ].height;
					current_tile->height[ 3 ] = vertices[ ( ( tile_y + 1 ) * 5 ) + tile_x + 1 ].height;

					current_tile->shading[ 0 ] = vertices[ ( tile_y * 5 ) + tile_x ].lighting;
					current_tile->shading[ 1 ] = vertices[ ( tile_y * 5 ) + tile_x + 1 ].lighting;
					current_tile->shading[ 2 ] = vertices[ ( ( tile_y + 1 ) * 5 ) + tile_x ].lighting;
					current_tile->shading[ 3 ] = vertices[ ( ( tile_y + 1 ) * 5 ) + tile_x + 1 ].lighting;
				}
			}
		}
	}

	plCloseFile( fh );

	Update();
}

void Terrain::LoadHeightmap( const std::string& path, int multiplier ) {
	PLImage image;
	if ( !plLoadImage( path.c_str(), &image ) ) {
		LogWarn( "Failed to load the specified heightmap, \"%s\" (%s)!\n", path.c_str(), plGetError() );
		return;
	}

	if ( image.width < 65 || image.height < 65 ) {
		plFreeImage( &image );
		LogWarn( "Invalid image size for heightmap, %dx%d vs 65x65!\n", image.width, image.height );
		return;
	}

	// Each channel is encoded with specific data
	// red = height
	// green = texture

	unsigned int chan_length = image.width * image.height;

	auto* rchan = static_cast<float*>(u_alloc( chan_length, sizeof( float ), true ));
	uint8_t* pixel = image.data[ 0 ];
	for ( unsigned int i = 0; i < chan_length; ++i ) {
		rchan[ i ] = static_cast<int>(*pixel) * multiplier; //(static_cast<int>(*pixel) - 127) * 256;
		pixel += 4;
	}

	auto* gchan = static_cast<uint8_t*>(u_alloc( chan_length, sizeof( uint8_t ), true ));
	pixel = image.data[ 0 ] + 1;
	for ( unsigned int i = 0; i < chan_length; ++i ) {
		gchan[ i ] = *pixel;
		pixel += 4;
	}

	plFreeImage( &image );

	for ( unsigned int chunk_y = 0; chunk_y < TERRAIN_CHUNK_ROW; ++chunk_y ) {
		for ( unsigned int chunk_x = 0; chunk_x < TERRAIN_CHUNK_ROW; ++chunk_x ) {
			Chunk& current_chunk = chunks_[ chunk_x + chunk_y * TERRAIN_CHUNK_ROW ];
			for ( unsigned int tile_y = 0; tile_y < TERRAIN_CHUNK_ROW_TILES; ++tile_y ) {
				for ( unsigned int tile_x = 0; tile_x < TERRAIN_CHUNK_ROW_TILES; ++tile_x ) {
					Tile* current_tile = &current_chunk.tiles[ tile_x + tile_y * TERRAIN_CHUNK_ROW_TILES ];
					unsigned int aaa = ( chunk_y * 4 * 65 ) + ( chunk_x * 4 );
					current_tile->height[ 0 ] = rchan[ aaa + ( tile_y * 65 ) + tile_x ];
					current_tile->height[ 1 ] = rchan[ aaa + ( tile_y * 65 ) + tile_x + 1 ];
					current_tile->height[ 2 ] = rchan[ aaa + ( ( tile_y + 1 ) * 65 ) + tile_x ];
					current_tile->height[ 3 ] = rchan[ aaa + ( ( tile_y + 1 ) * 65 ) + tile_x + 1 ];

					current_tile->texture = gchan[ aaa + ( tile_y * 65 ) + tile_x ];

					// hrm...
					current_tile->shading[ 0 ] =
					current_tile->shading[ 1 ] =
					current_tile->shading[ 2 ] =
					current_tile->shading[ 3 ] = 255;
				}
			}

			max_height_ = min_height_ = current_chunk.tiles[ 0 ].height[ 0 ];

			// Find the maximum and minimum points
			for ( auto& tile : current_chunk.tiles ) {
				for ( float i : tile.height ) {
					if ( i > max_height_ ) {
						max_height_ = i;
					}
					if ( i < min_height_ ) {
						min_height_ = i;
					}
				}
			}
		}
	}

	u_free( rchan );
	u_free( gchan );

	Update();
}
