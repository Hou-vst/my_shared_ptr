#pragma once
#include<mutex>

template<typename T>
class my_ref_count_manager
{
private:
	int m_use_count;
	int m_weak_count;
	T* m_ptr;
	std::mutex m_mutex;

public:
	my_ref_count_manager(T* ptr)
		:m_use_count(1), m_weak_count(1),m_ptr(ptr)
	{
		
	}

	virtual ~my_ref_count_manager()
	{

	}

private:
	void Delete()
	{
		if (m_ptr)
		{
			delete m_ptr;
			m_ptr = NULL;
		}
	}

	void Delete_this()
	{
		delete this;
	}

protected:
	bool IncrRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_use_count <= 0)
		{
			return false;
		}

		m_use_count++;
		return true;
	}

	bool DecRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_use_count <= 0)
		{
			return false;
		}

		m_use_count--;
		if (m_use_count == 0)
		{
			Delete();
		}

		return true;
	}

	bool IncrWeakRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_weak_count <= 0)
		{
			return false;
		}

		m_weak_count++;
		return true;
	}

	void DecWeakRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_weak_count <= 0)
		{
			return;
		}

		m_weak_count--;
		if (m_weak_count == 0)
		{
			Delete_this();
		}
	}
};

template<typename T>
class my_shared_ptr : public my_ref_count_manager<T>
{

public:
	my_shared_ptr():my_ref_count_manager(NULL)
	{

	}

	//问题1，传入的参数需不需要指定T
	my_shared_ptr(const my_shared_ptr& other)
	{
		
	}

	my_shared_ptr(my_shared_ptr<T>&& right)
	{

	}
};
