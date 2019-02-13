# RBTMemAlloc
Simple red black tree based memory allocator using C++11 and above.
## Getting Started
### Setup
Just copy RBTMemoryAllocator.h and RBTMemoryAllocator.cpp onto your computer and add them to your project.
### Basic example
The simplest way to use the allocator is to create an instance of the RBTMemoryAllocator class.
```cpp
RBTMemoryAllocator allocator;
```
Now you are ready to go. Just use the ```allocate``` and ```deallocate``` methods.
```cpp
char* myString = allocator.allocate(512);
allocator.deallocate(myString);
```
If you want to apply a specific alignment to the allocated memory, just pass it as a second argument of ```allocate```.
```cpp
float* sseVectorData = allocator.allocate(sizeof(float) * 4, 16);
```
## More Advanced
### Changing the memory size
Implicitly, the allocator's constructor allocates 8 MB of memory to use. If you want to change that pass the new memory size to use to the constructor.
```cpp
RBTMemoryAllocator allocator(2 * RBTMemoryAllocator::MegaByte);
```
Also, the allocator contains a static instance of the allocator. It also consumes 8 MB, but it is currently immutable.
### Providing your own memory
RBTMemAlloc allows you to provide your own memory to the allocator. Keep in mind it won't deallocate this memory.
```cpp
char myBuffer[RBTMemoryAllocator::KiloByte * 8];
RBTMemoryAllocator allocator(myBuffer, sizeof(myBuffer));
```
### Using STL
RBTMemAlloc partially supports the STL library by ```StdAllocator<T>``` template class. Most common aliases for for instance std::vector and std::string are provided. Keep in mind that StdAllocator uses internally RBTMemoryAllocator::instance to allocate memory, but it's a subject to change.
```cpp
String string;
Vector<char> vec;
```
## License
This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.
