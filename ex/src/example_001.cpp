//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_001
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include "mce.hpp"

void read_file_content(std::string fname, mce::chan<int> done_ch) {
    std::string fileContent;

    // create a function to execute in mce::await() that accesses the caller's stack 
    auto read_file = [&] {
        std::ifstream file(fname); // open file

        if(file.is_open()) { 
            std::ostringstream ss;
            ss << file.rdbuf(); // extract file contents
            fileContent = ss.str();
            return true; // await() will return what the input function returns
        } else { 
            return false; 
        }
    };
    
    // wait for boolean return value of read_file function
    if(mce::await(read_file)) { 
        std::cout << "file successfully read" << std::endl; 
    } else { 
        std::cout << "failed to read file" << std::endl; 
    }

    // print the file content 
    std::cout << "fileContent: " << fileContent << std::endl;
    done_ch.send(0);
}

int main(int argc, char** argv) {
    // this is test setup, just ensuring there is a file to read
    std::string fname("my_filename.txt");
    std::ofstream file(fname, std::ios_base::trunc);

    if(file) {
        file << "hello world!";
    }

    file.close();

    // launch asynchronous coroutine to read the file content 
    auto done_ch = mce::chan<int>::make(); 
    mce::parallel(read_file_content, fname, done_ch);

    // wait for coroutine to finish before the program exits
    int r;
    done_ch.recv(r);
    return 0;
}
