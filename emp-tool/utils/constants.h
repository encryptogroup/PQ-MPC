#ifndef CONFIG_H__
#define CONFIG_H__
namespace emp {
const static int AES_BATCH_SIZE = 2048;

const static int PUBLIC = 0;
const static int ALICE = 1;
const static int BOB = 2;

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
#define UNIX_PLATFORM
#endif
}
#endif // CONFIG_H__
