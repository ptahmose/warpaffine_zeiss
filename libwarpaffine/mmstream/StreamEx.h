// SPDX-FileCopyrightText: 2023 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: MIT

#pragma once
#include <memory>
#include <map>
#include <string>
#include "IStreamEx.h"

/// Creates a stream object (implementing the IStreamEx interface) for the specified arguments : the filename (can be an URL or similar,
/// depending on the stream class), the stream class (if empty, the stock file stream object is used) and a property bag (containing
/// additional stream class specific parameters).
/// This function will either return a valid stream object, or throw an exception.
///
/// \param  filename        Filename ((can be a URL or similar, depending on the stream class) of the file.
/// \param  stream_class    The stream class, if empty, the stock file stream object is used.
/// \param  property_bag    The property bag.
///
/// \returns    If successful, the newly created stream object.
std::shared_ptr<IStreamEx> CreateStockStreamEx(const wchar_t* filename, const std::string& stream_class, const std::map<int, libCZI::StreamsFactory::Property>& property_bag);
