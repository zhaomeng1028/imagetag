#ifndef __BUFFER_FILE__
#define __BUFFER_FILE__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// Read file to buffer
class BufferFile {
 public :
    std::string file_path_;
    int length_;
    char* buffer_;

    BufferFile(std::string file_path);
    ~BufferFile();
    int GetLength();
    char* GetBuffer();
};

#endif
