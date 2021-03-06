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

#include "../../engine.h"

/////////////////////////////////////////////////////////////
// Newton Dynamics Interface
// This should never be exposed to the rest of the engine!

#include <Newton.h>

class NTPhysicsBody : public IPhysicsBody {
public:
	NTPhysicsBody() {}
	~NTPhysicsBody() override {
		NewtonDestroyCollision( newtonCollision );
	}

protected:
private:
	NewtonCollision *newtonCollision{ nullptr };
	NewtonBody *newtonBody{ nullptr };
};

class NTPhysicsInterface : public IPhysicsInterface {
public:
	NTPhysicsInterface();
	~NTPhysicsInterface() override;

	void Tick() override;

	IPhysicsBody *CreatePhysicsBody() override;
	void DestroyPhysicsBody( IPhysicsBody *body ) override;

	void GenerateTerrainCollision( std::vector<float> vertices ) override;
	void DestroyTerrainCollision() override;

protected:
private:
	static void *AllocMemory( int size );
	static void FreeMemory( void *ptr, int size );

	NewtonWorld *newtonWorld{ nullptr };
	NewtonCollision *terrainCollision{ nullptr };
};

IPhysicsInterface *IPhysicsInterface::CreateInstance() {
	return new NTPhysicsInterface();
}

void IPhysicsInterface::DestroyInstance( IPhysicsInterface *instance ) {
	delete instance;
}

/////////////////////////////////////////////////////////////

void *NTPhysicsInterface::AllocMemory( int size ) {
	return u_alloc( 1, size, true );
}

void NTPhysicsInterface::FreeMemory( void *ptr, int size ) {
	u_unused( size );
	u_free( ptr );
}

NTPhysicsInterface::NTPhysicsInterface() {
	newtonWorld = NewtonCreate();
	NewtonSetMemorySystem( NTPhysicsInterface::AllocMemory, NTPhysicsInterface::FreeMemory );
}

NTPhysicsInterface::~NTPhysicsInterface() {
	NewtonDestroyAllBodies( newtonWorld );
	NewtonDestroy( newtonWorld );
}

void NTPhysicsInterface::Tick() {
	NewtonUpdate( newtonWorld, 1.0f / 25.0f ); // todo: double check this...
}

IPhysicsBody *NTPhysicsInterface::CreatePhysicsBody() {
	auto *body = new NTPhysicsBody();
	return body;
}

void NTPhysicsInterface::DestroyPhysicsBody( IPhysicsBody *body ) {
	delete dynamic_cast<NTPhysicsBody *>(body);
}

/////////////////////////////////////////////////////////////
// Terrain

void NTPhysicsInterface::GenerateTerrainCollision( std::vector<float> vertices ) {
#if 0
	const unsigned int size = 65;
	terrain_collision_ = NewtonCreateHeightFieldCollision(
		newton_world_,
		size, size,
		0, 0,
		vertices.data(),
		);
#endif
}

void NTPhysicsInterface::DestroyTerrainCollision() {
	NewtonDestroyCollision( terrainCollision );
}
