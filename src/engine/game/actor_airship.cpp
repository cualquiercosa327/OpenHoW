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

#include "actor_manager.h"
#include "actor_airship.h"

REGISTER_ACTOR( airship, AAirship )

using namespace openhow;

AAirship::AAirship() : SuperClass() {}

AAirship::~AAirship() {
	delete ambientSource;
}

void AAirship::Tick() {
	SuperClass::Tick();

	ambientSource->SetPosition( GetPosition() );
}

void AAirship::Deserialize( const ActorSpawn &spawn ) {
	SuperClass::Deserialize( spawn );

	ambientSource = Engine::Audio()->CreateSource( "audio/en_bip.wav",
												   { 0.0f, 0.0f, 0.0f },
												   { 0.0f, 0.0f, 0.0f },
												   true,
												   1.0f,
												   1.0f,
												   true );
	ambientSource->StartPlaying();

	SetModel( "scenery/airship1" );
	SetAngles( { 180.0f, 0.0f, 0.0f } );
	ShowModel( true );
}
