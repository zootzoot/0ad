/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "simulation2/system/Component.h"
#include "ICmpMinimap.h"

#include "simulation2/components/ICmpPlayerManager.h"
#include "simulation2/components/ICmpPlayer.h"
#include "simulation2/components/ICmpOwnership.h"
#include "simulation2/MessageTypes.h"

#include "ps/Overlay.h"
#include "ps/Game.h"
#include "ps/ConfigDB.h"

#include "lib/timer.h"

class CCmpMinimap : public ICmpMinimap
{
public:
	static void ClassInit(CComponentManager& componentManager)
	{
		componentManager.SubscribeToMessageType(MT_PositionChanged);
		componentManager.SubscribeToMessageType(MT_OwnershipChanged);
		componentManager.SubscribeToMessageType(MT_PingOwner);
	}

	DEFAULT_COMPONENT_ALLOCATOR(Minimap)

	// Template state:

	bool m_UsePlayerColour;

	u8 m_R, m_G, m_B; // static template state if m_UsePlayerColour false; dynamic state if true

	// Dynamic state:

	bool m_Active;
	entity_pos_t m_X, m_Z; // cache the latest position for more efficient rendering; only valid when m_Active true


	// Used by the minimap while pinging this entity to indicate something (e.g. an attack)
	// Entity not pinged after m_PingDuration unless its notified in between
	double m_PingDuration;
	double m_BlinkDuration;
	double m_HalfBlinkDuration;
	double m_PingStartTime;
	double m_LastBlinkStartTime;
	bool m_IsPinging;

	static std::string GetSchema()
	{
		return
			"<element name='Type'>"
				"<choice>"
					"<value>food</value>"
					"<value>wood</value>"
					"<value>stone</value>"
					"<value>metal</value>"
					"<value>structure</value>"
					"<value>settlement</value>"
					"<value>unit</value>"
					"<value>support</value>"
					"<value>hero</value>"
				"</choice>"
			"</element>"
			"<optional>"
				"<element name='Colour'>"
					"<attribute name='r'>"
						"<data type='integer'><param name='minInclusive'>0</param><param name='maxInclusive'>255</param></data>"
					"</attribute>"
					"<attribute name='g'>"
						"<data type='integer'><param name='minInclusive'>0</param><param name='maxInclusive'>255</param></data>"
					"</attribute>"
					"<attribute name='b'>"
						"<data type='integer'><param name='minInclusive'>0</param><param name='maxInclusive'>255</param></data>"
					"</attribute>"
				"</element>"
			"</optional>";
	}

	virtual void Init(const CParamNode& paramNode)
	{
		m_Active = true;
		m_IsPinging = false;

		const CParamNode& active = paramNode.GetChild("active");
		if (active.IsOk())
		{
			m_Active = active.GetChild("@active").ToBool();

			const CParamNode& pinging = paramNode.GetChild("pinging");
			if (pinging.IsOk())
				m_IsPinging = pinging.GetChild("@pinging").ToBool();
		}

		// Ping blinking durations
		m_PingStartTime = 0;
		m_LastBlinkStartTime = m_PingStartTime;

		// Tests won't have config initialised
		if (CConfigDB::IsInitialised())
		{
			CFG_GET_VAL("pingduration", Double, m_PingDuration);
			CFG_GET_VAL("blinkduration", Double, m_BlinkDuration);
		}
		else
		{
			m_PingDuration = 25.f;
			m_BlinkDuration = 1.f;
		}

		m_HalfBlinkDuration = m_BlinkDuration/2;

		const CParamNode& colour = paramNode.GetChild("Colour");
		if (colour.IsOk())
		{
			m_UsePlayerColour = false;
			m_R = (u8)colour.GetChild("@r").ToInt();
			m_G = (u8)colour.GetChild("@g").ToInt();
			m_B = (u8)colour.GetChild("@b").ToInt();
		}
		else
		{
			m_UsePlayerColour = true;
			// Choose a bogus colour which will get replaced once we have an owner
			m_R = 255;
			m_G = 0;
			m_B = 255;
		}
	}

	virtual void Deinit()
	{
	}

	template<typename S>
	void SerializeCommon(S& serialize)
	{
		if (m_UsePlayerColour)
		{
			serialize.NumberU8_Unbounded("r", m_R);
			serialize.NumberU8_Unbounded("g", m_G);
			serialize.NumberU8_Unbounded("b", m_B);
		}

		serialize.Bool("active", m_Active);

		if (m_Active)
		{
			serialize.NumberFixed_Unbounded("x", m_X);
			serialize.NumberFixed_Unbounded("z", m_Z);

			// Serialize the ping state
			serialize.Bool("pinging", m_IsPinging);
		}
	}

	virtual void Serialize(ISerializer& serialize)
	{
		SerializeCommon(serialize);
	}

	virtual void Deserialize(const CParamNode& paramNode, IDeserializer& deserialize)
	{
		Init(paramNode);

		SerializeCommon(deserialize);
	}

	virtual void HandleMessage(const CMessage& msg, bool UNUSED(global))
	{
		switch (msg.GetType())
		{
		case MT_PositionChanged:
		{
			const CMessagePositionChanged& data = static_cast<const CMessagePositionChanged&> (msg);

			if (data.inWorld)
			{
				m_Active = true;
				m_X = data.x;
				m_Z = data.z;
			}
			else
			{
				m_Active = false;
			}

			break;
		}
		case MT_OwnershipChanged:
		{
			if (!m_UsePlayerColour)
				break;

			const CMessageOwnershipChanged& msgData = static_cast<const CMessageOwnershipChanged&> (msg);

			// If there's no new owner (e.g. the unit is dying) then don't try updating the colour
			if (msgData.to == -1)
				break;

			// Find the new player's colour
			CmpPtr<ICmpPlayerManager> cmpPlayerManager(GetSimContext(), SYSTEM_ENTITY);
			if (!cmpPlayerManager)
				break;
			CmpPtr<ICmpPlayer> cmpPlayer(GetSimContext(), cmpPlayerManager->GetPlayerByID(msgData.to));
			if (!cmpPlayer)
				break;
			CColor colour = cmpPlayer->GetColour();
			m_R = (u8)(colour.r*255.0);
			m_G = (u8)(colour.g*255.0);
			m_B = (u8)(colour.b*255.0);
			// TODO: probably should avoid using floating-point here

			break;
		}
		case MT_PingOwner:
		{
			// UNUSED: const CMessagePingOwner& data = static_cast<const CMessagePingOwner&> (msg);

			if (!g_Game)
				break;

			CmpPtr<ICmpOwnership> cmpOwnership(GetSimContext(), GetEntityId());
			if (!cmpOwnership)
				break;
			if(g_Game->GetPlayerID() != (int)cmpOwnership->GetOwner())
				break;

			m_Active = true;
			m_IsPinging = true;
			m_PingStartTime = timer_Time();
			m_LastBlinkStartTime = m_PingStartTime;

			break;
		}
		}
	}

	virtual bool GetRenderData(u8& r, u8& g, u8& b, entity_pos_t& x, entity_pos_t& z)
	{
		if (!m_Active)
			return false;

		r = m_R;
		g = m_G;
		b = m_B;
		x = m_X;
		z = m_Z;
		return true;
	}

	virtual bool IsPinging(void)
	{
		if (!m_Active)
			return false;

		return m_IsPinging;
	}

	virtual bool CheckPing(void)
	{
		double currentTime = timer_Time();
		double dt = currentTime - m_PingStartTime;
		if (dt > m_PingDuration)
		{
			m_IsPinging = false;
			m_PingStartTime = 0;
			m_LastBlinkStartTime = m_PingStartTime;
		}

		dt = currentTime - m_LastBlinkStartTime;
		// Return true for dt > 0 && dt < m_HalfBlinkDuration
		if (dt < m_HalfBlinkDuration)
			return true;

		// Reset if this blink is complete
		if (dt > m_BlinkDuration)
			m_LastBlinkStartTime = currentTime;

		// Return false for dt >= m_HalfBlinkDuration && dt < m_BlinkDuration
		return false;
	}
};

REGISTER_COMPONENT_TYPE(Minimap)
