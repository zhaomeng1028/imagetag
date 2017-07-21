#include <BufferFile.h>
#include <assert.h>
#include <iostream>
#include <typeinfo>
BufferFile:: BufferFile(std::string file_path)
    :file_path_(file_path) {

    std::ifstream ifs(file_path.c_str(), std::ios::in | std::ios::binary);
    if (!ifs) {
        //std::cerr << "Can't open the file. Please check " << file_path << ". \n";
        fprintf(stderr,"Can't open the file. Please check %s.\n",file_path.c_str());
        assert(false);
    }

    ifs.seekg(0, std::ios::end);
    length_ = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    //std::cout << file_path.c_str() << " ... "<< length_ << " bytes\n";
    printf("%s ... %d bytes\n",file_path.c_str(),length_);
    buffer_ = new char[sizeof(char) * length_];
    ifs.read(buffer_, length_);
    ifs.close();
}

int BufferFile::GetLength() {
    return length_;
}

char* BufferFile::GetBuffer() {
    return buffer_;
}

BufferFile::~BufferFile() {
    delete[] buffer_;
    buffer_ = NULL;
}
