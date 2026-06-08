// Copyright (c) 2018-2024 Charlie Vanaret
// Licensed under the MIT license. See LICENSE file in the project directory for details.

#ifndef UNO_LOGGER_H
#define UNO_LOGGER_H

#include <string>
#include <iostream>
#include "tools/r_io.h"

namespace uno {
   enum Level {
       SILENT = 0, DISCRETE, WARNING, INFO, DEBUG, DEBUG2, DEBUG3
   };

   class Logger {
   public:
       static Level level;
       inline static std::ostream* stream = &UNO_COUT;
       static void set_logger(const std::string& logger_level);
       static void set_stream(std::ostream& output_stream);
       static void flush();
   };

   template <typename T>
   const Level& operator<<(const Level& level, T& element) {
      if (level <= Logger::level) {
         (*Logger::stream) << element;
      }
      return level;
   }

   template <typename T>
   const Level& operator<<(const Level& level, const T& element) {
      if (level <= Logger::level) {
         (*Logger::stream) << element;
      }
      return level;
   }

   const Level& operator<<(const Level& level, std::ostream& (*element)(std::ostream&));
   
} // namespace

#endif // UNO_LOGGER_H