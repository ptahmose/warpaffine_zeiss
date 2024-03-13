// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once

#include <limits>

/// This is representing a brick-coordinate, i.e. a scheme in order to uniquely identify a brick
/// within a document.
struct BrickCoordinate
{
    int t;
    int c;

    BrickCoordinate(int t, int c) :t(t), c(c) {}
    BrickCoordinate() : t(0), c(0) {}

    /// Less-than comparison operator - required to use this struct as a key in a map.
    /// \param  other The object to compare with.
    /// \returns {bool} True if the current object appears before the specified object.
    bool operator<(const BrickCoordinate& other) const
    {
        // t has highest precedence
        return this->t < other.t || (this->t == other.t && this->c < other.c);
    }

    bool operator==(const BrickCoordinate& other) const
    {
        return this->t == other.t && this->c == other.c;
    }

    bool operator!= (const BrickCoordinate& other) const
    {
        return this->t != other.t || this->c != other.c;
    }

    void MakeInvalid()
    {
        this->t = this->c = (std::numeric_limits<int>::min)();
    }
};
