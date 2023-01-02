/*
  BHFS: Black Hole Filesystem
  
  Copyright (C) 2017-2023	   David Turco  	

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

enum { 
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

void bhfs_log(int level, const char *message, ...);
