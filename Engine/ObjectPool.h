#ifndef OBJECT_POOL_H_
#define OBJECT_POOL_H_

#include <vector>

template <typename T>
class ObjectPool
{
protected:
	T* objPtr;
	std::vector<uint32_t> freeObjs;
	size_t poolSize;

public:

	ObjectPool(size_t initialSize);
	~ObjectPool();

	void Resize(size_t size);
	void Grow(size_t size);
	T* Alloc();
	void Dealloc(const T* obj);
	size_t GetSize() const;
};

template<typename T>
inline ObjectPool<T>::ObjectPool(size_t initialSize) :
	objPtr(new T[initialSize]),
	poolSize(initialSize)
{
	freeObjs.reserve(initialSize);
	for (size_t i = 0; i < initialSize; ++i)
		freeObjs.push(i);
}

template<typename T>
inline ObjectPool<T>::~ObjectPool()
{
	delete[] objPtr;
}

template<typename T>
inline void ObjectPool<T>::Resize(size_t size)
{
	delete[] objPtr;
	objPtr = new T[size];

	freeObjs.clear();
	freeObjs.reserve(size);
	for (size_t i = 0; i < initialSize; ++i)
		freeObjs.push(i);

	poolSize = size;
}

template<typename T>
inline void ObjectPool<T>::Grow(size_t size)
{
	auto newPtr = new T[size];
	memcpy(newPtr, objPtr, size * sizeof(T));
	delete[] objPtr;
	objPtr = newPtr;

	for (size_t i = poolSize; i < size; ++i)
		freeObjs.push_back(i);

	poolSize = size;
}

template<typename T>
inline T * ObjectPool<T>::Alloc()
{
	auto loc = freeObjs[freeObjs.size() - 1];
	freeObjs.pop_back();
	return &objPtr[loc];
}

template<typename T>
inline void ObjectPool<T>::Dealloc(const T* obj)
{
	freeObjs.push_back(obj - objPtr);
}

template<typename T>
inline size_t ObjectPool<T>::GetSize() const
{
	return poolSize;
}

#endif