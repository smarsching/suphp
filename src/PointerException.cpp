/*
    suPHP - (c)2002-2005 Sebastian Marsching <sebastian@marsching.com>

    This file is part of suPHP.

    suPHP is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    suPHP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with suPHP; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "PointerException.hpp"

using namespace suPHP;

std::string suPHP::PointerException::getName() const {
    return "PointerException";
}

suPHP::PointerException::PointerException(std::string file, int line) : 
    Exception(file, line) {}

suPHP::PointerException::PointerException(std::string message, std::string file, int line) :
    Exception(message, file, line) {}

suPHP::PointerException::PointerException(Exception& cause, std::string file, int line) : 
    Exception(cause, file, line) {}

suPHP::PointerException::PointerException(std::string message, Exception& cause, std::string file, int line) : 
    Exception(message, cause, file, line) {}
