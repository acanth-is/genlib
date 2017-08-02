// Copyright (C) 2017 Christian Sailer

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "stringutils.h"
#include <sstream>
#include <memory>

namespace dXstring {
    std::vector<std::string> split(const std::string &s, char delim)
    {
        std::vector<std::string> elems;
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (std::getline(ss, item, delim))
        {
            elems.push_back(item);
        }

        return elems;
    }

    std::string readString(std::istream & stream)
    {
        unsigned int length;
        stream.read(reinterpret_cast<char *>(&length), sizeof(length));
        if ( length == 0)
        {
            return std::string();
        }
        std::string result( length, '\0');
        char *ptr  = &result[0];
        stream.read(ptr, length);
        return result;
    }

    void writeString(std::ostream &stream, const std::string &s)
    {
        unsigned int length = s.length();
        stream.write(reinterpret_cast<char *>(&length), sizeof(unsigned int));
        if (length > 0)
        {
            stream.write(s.data(), length);
        }
    }

    std::string formatString(double value, const char *format)
    {
        std::unique_ptr<char[]> buffer(new char[24 + strlen(format)]);
        sprintf( buffer.get(), format, value );
        return std::string(buffer.get());
    }


}
