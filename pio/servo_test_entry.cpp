// PlatformIO entry point for the `servo_test` environment.
//
// PlatformIO only converts .ino files that live in src_dir (which points at
// the game sketch), so this thin wrapper pulls in the standalone test sketch.
// It lives outside servo_test/ on purpose: the Arduino IDE compiles every
// .cpp inside a sketch folder, which would define everything twice.
#include "../servo_test/servo_test.ino"
