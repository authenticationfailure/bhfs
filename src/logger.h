enum { 
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_DEBUG
};

void bhfs_log(int level, const char *message, ...);
