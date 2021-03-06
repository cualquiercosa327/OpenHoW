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

#include "../engine.h"
#include "../frontend.h"

#include "actor_manager.h"
#include "actor.h"

/************************************************************/

ActorSet ActorManager::actors_;
std::vector<Actor *> ActorManager::destructionQueue;
std::map<std::string, ActorManager::actor_ctor_func> ActorManager::actor_classes_
	__attribute__((init_priority (1000)));

Actor *ActorManager::CreateActor( const std::string &class_name, const ActorSpawn &spawnData ) {
	auto i = actor_classes_.find( class_name );
	if ( i == actor_classes_.end() ) {
		// TODO: make this throw an error rather than continue...
		LogWarn( "Failed to find actor class %s!\n", class_name.c_str() );
		return nullptr;
	}

	Actor *actor = i->second();
	actors_.insert( actor );

	actor->Deserialize( spawnData );

	return actor;
}

void ActorManager::DestroyActor( Actor *actor ) {
	u_assert( actor != nullptr, "attempted to delete a null actor!\n" );

	// Ensure it's not already queued for destruction
	if ( std::find( destructionQueue.begin(), destructionQueue.end(), actor ) != destructionQueue.end() ) {
		LogDebug( "Attempted to queue actor for deletion twice, ignoring...\n" );
		return;
	}

	// Move it into a queue for destruction
	destructionQueue.push_back( actor );
}

void ActorManager::TickActors() {
	for ( auto const &actor: actors_ ) {
		if ( !actor->IsActivated() ) {
			continue;
		}

		actor->Tick();
	}

	// Now clean everything up that was marked for destruction
	for ( auto &i : destructionQueue ) {
		assert( actors_.find( i ) != actors_.end() );

		actors_.erase( i );
		delete i;
	}
	destructionQueue.clear();
}

void ActorManager::DrawActors() {
	if ( FrontEnd_GetState() == FE_MODE_LOADING ) {
		return;
	}

	g_state.gfx.num_actors_drawn = 0;
	for ( auto const &actor: actors_ ) {
		if ( cv_graphics_cull->b_value && !actor->IsVisible() ) {
			continue;
		}

		g_state.gfx.num_actors_drawn++;

		actor->Draw();
	}
}

void ActorManager::DestroyActors() {
	for ( auto &actor: actors_ ) {
		delete actor;
	}

	destructionQueue.clear();
	actors_.clear();
}

void ActorManager::ActivateActors() {
	for ( auto const &actor: actors_ ) {
		actor->Activate();
	}
}

void ActorManager::DeactivateActors() {
	for ( auto const &actor: actors_ ) {
		actor->Deactivate();
	}
}

ActorManager::ActorClassRegistration::ActorClassRegistration( const std::string &name, actor_ctor_func ctor_func )
	: name_( name ) {
	ActorManager::actor_classes_[ name ] = ctor_func;
}

ActorManager::ActorClassRegistration::~ActorClassRegistration() {
	ActorManager::actor_classes_.erase( name_ );
}
