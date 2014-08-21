/*******************************************************************************
 *               Atrinik, a Multiplayer Online Role Playing Game               *
 *                                                                             *
 *       Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team       *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program; if not, write to the Free Software Foundation, Inc.,     *
 * 675 Mass Ave, Cambridge, MA 02139, USA.                                     *
 *                                                                             *
 * The author can be reached at admin@atrinik.org                              *
 ******************************************************************************/

/**
 * @file
 * Logger.
 */

#pragma once

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/exception/enable_error_info.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/log/support/exception.hpp>

namespace atrinik {

class Logger {
public:

    enum Level {
        Development,
        Detail,
        Info,
        Error,
        Critical
    };

    typedef boost::log::sources::severity_logger_mt<Level> LoggerType;

    static void init();
    static LoggerType logger;
};

#define LOG(x) BOOST_LOG_SEV(Logger::logger, Logger:: x)
#define LOG_EXCEPTION(x) boost::enable_error_info(x) << \
        boost::log::current_scope()
#define LOG_STACK(x) "\n\tScope stack:\n\t" << \
        *boost::get_error_info<boost::log::current_scope_info>(x)

};
