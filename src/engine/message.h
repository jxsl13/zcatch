/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_MESSAGE_H
#define ENGINE_MESSAGE_H

#include <base/system.h>
#include <engine/shared/packer.h>

class CMsgPacker : public CPacker
{
public:
	CMsgPacker(int Type, bool System=false)
	{
		Reset();
		AddInt((Type<<1)|(System?1:0));
	}

	// copy constructor
	CMsgPacker(const CMsgPacker &other)
	{
		mem_copy(m_aBuffer, other.m_aBuffer, PACKER_BUFFER_SIZE);
		size_t offset = other.m_pCurrent - other.m_aBuffer;
		m_pCurrent = m_aBuffer + offset;
		m_pEnd = m_aBuffer + PACKER_BUFFER_SIZE;
	}

	CMsgPacker& operator=(const CMsgPacker& other)
    {
		if (&other != this)
		{
			mem_copy(m_aBuffer, other.m_aBuffer, PACKER_BUFFER_SIZE);
			size_t offset = other.m_pCurrent - other.m_aBuffer;
			m_pCurrent = m_aBuffer + offset;
			m_pEnd = m_aBuffer + PACKER_BUFFER_SIZE;
		}
		return *this;
	}
};

#endif
