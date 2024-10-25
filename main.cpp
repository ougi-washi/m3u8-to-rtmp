// MTR https://github.com/ougi-washi/m3u8-to-rtmp

#include "handles.h"
#include <iostream>

i32 main(i32 argc, char const *argv[])
{
    mtr::info info = mtr::get_info("config.json");

    mtr::handle* handle = new mtr::handle();

    if (!mtr::init(handle, info))
    {
        std::cerr << "Error: Could not initialize handle" << std::endl;
        return -1;
    }

    mtr::process(handle);
    mtr::cleanup(handle);
    return 0;
}
