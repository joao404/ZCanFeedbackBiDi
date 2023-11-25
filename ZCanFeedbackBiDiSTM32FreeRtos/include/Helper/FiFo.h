/*********************************************************************
 * FiFo
 *
 * Copyright (C) 2023 Marcel Maage
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#pragma once

#include <array>

// FiFo buffer with fixed size
// Underlying container is a std::array as circular buffer

template <class TYPE, std::size_t MEM_SIZE>
class FiFo
{
public:
    FiFo(){};
    virtual ~FiFo(){};

    bool isEmpty()
    {
        return 0 == m_count;
    }

    bool isFull()
    {
        return MEM_SIZE == m_count;
    }

    size_t count()
    {
        return m_count;
    }

    // removes the first element of the FIFO
    // returns true if successful
    bool front(TYPE &data)
    {
        bool returnValue{false};
        if (!isEmpty())
        {
            returnValue = true;
            data = *m_output;
        }
        return returnValue;
    }

    // removes the first element of the FIFO
    // returns true if successful
    bool pop()
    {
        bool returnValue{false};
        if (!isEmpty())
        {
            returnValue = true;
            if (m_output == &m_buffer[MEM_SIZE - 1])
            {
                // already last element of buffer => switch to first element
                m_output = &m_buffer[0];
            }
            else
            {
                m_output++;
            }
            m_count--;
        }
        return returnValue;
    }

    // adds an element at the end of the FIFO
    // returns true if successful
    bool emplace(TYPE &data)
    {
        bool returnValue{false};
        if (MEM_SIZE > m_count)
        {
            // we can at least save one more element
            *m_nextInput = data;
            if (m_nextInput == &m_buffer[MEM_SIZE - 1])
            {
                // already last element of buffer => switch to first element
                m_nextInput = &m_buffer[0];
            }
            else
            {
                m_nextInput++;
            }
            m_count++;
            returnValue = true;
        }
        return returnValue;
    }

private:
    std::array<TYPE, MEM_SIZE> m_buffer;

    // points to element that return when
    TYPE *m_output{&m_buffer[0]};
    // points to next element that is written
    TYPE *m_nextInput{&m_buffer[0]};
    // number of elements in FIFO
    size_t m_count{0};
};