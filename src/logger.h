static int LOG_NONE = 0;
static int LOG_ERROR = 1;
static int LOG_WARNING = 2;
static int LOG_INFO = 3;
static int LOG_DEBUG = 4;

void bhfs_log(int level, const char *message, ...);
