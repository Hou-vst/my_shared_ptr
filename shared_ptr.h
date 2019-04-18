#pragma once
#include<mutex>

template<typename T>
class my_ref_count_base
{
private:
	int m_use_count;
	int m_weak_count;
	T* m_ptr;
	std::mutex m_mutex;

public:
	my_ref_count_base(T* ptr)
		:m_use_count(1), m_weak_count(1),m_ptr(ptr)
	{
		
	}

	virtual ~my_ref_count_base()
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
	bool IncrRef_nz()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (m_use_count <= 0)
		{
			return false;
		}

		m_use_count++;
		return true;
	}

	void IncrRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_use_count++;
	}

	void DecRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_use_count--;
		if (m_use_count == 0)
		{
			//删除被管理的资源
			Delete();
			//猜测，弱引用初始化时是1，为了防止管理资源类不释放，要在被管理资源被释放时，减少弱引用计数
			DecWeakRef();
		}
	}

	bool IncrWeakRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_weak_count++;
	}

	void DecWeakRef()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_weak_count--;
		if (m_weak_count == 0)
		{
			Delete_this();
		}
	}

	int getUseCount()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_use_count;
	}
};

template<typename T2>
class my_shared_ptr;

template<typename T2>
class my_weak_ptr;

template<typename T>
class my_ptr_base
{
public:
	my_ptr_base():
		m_count_base(NULL)
		,m_ptr(NULL)
	{

	}

	int use_count()
	{
		if (m_count_base)
		{
			return m_count_base->getUseCount();
		}

		return 0;
	}

protected:
	T * get()
	{
		return m_ptr;
	}

	//右值构造不涉及引用计数的变化，所以不用区分是shared_ptr还是weak_ptr，直接操作即可
	template<class T2>
	void move_construct_from(my_ptr_base<T2>&& right)
	{
		m_ptr = right.m_ptr;
		m_count_base = right.m_count_base;

		right.m_ptr = NULL;
		right.m_count_base = NULL;
	}

	//适用于构造shared_ptr对象
	template<class T2>
	void copy_construct_from(my_shared_ptr<T2>& other)
	{
		if (other.m_count_base)
		{
			other.m_count_base->IncrRef();
		}

		m_ptr = other.m_ptr;
		m_count_base = other.m_count_base;
	}

	//适用于构造shared_ptr对象
	template<class T2>
	bool copy_form_weak(const my_weak_ptr<T2>& other)
	{
		//适用于weak_ptr构造share_ptr，因为管理类的生命周期大于被管理类，所以存在
		//管理类还存在但是被管理类已经析构也就是use_count为0的情况，此时use_count增加时需要判断下这种情况
		if (other.m_count_base && other.m_count_base->IncrRef_nz())
		{
			m_ptr = other.m_ptr;
			m_count_base = other.m_count_base;
			return true;
		}

		return false;
	}

	//适用于构造weak_ptr对象，传入的参数可能是shared_ptr也可能是weak_ptr
	template<class T2>
	void Weakly_construct_from(const my_ptr_base<T2>& other)
	{
		if (other.m_count_base)
		{
			other.m_count_base->IncrWeakRef();
		}

		m_ptr = other.m_ptr;
		m_count_base = other.m_count_base;
	}

	void DecRef()
	{
		if (m_count_base)
		{
			m_count_base->DecRef();
		}
	}

	void Dec_weak_Ref()
	{
		if (m_count_base)
		{
			m_count_base->DecWeakRef();
		}
	}

private:
	//管理类需要持有被管理类的指针
	my_ref_count_base<T> * m_count_base;
	T* m_ptr;
};

template<typename T>
class my_shared_ptr : public my_ptr_base<T>
{
	my_shared_ptr()
	{

	}

	my_shared_ptr(T* t)
	{
		m_ptr = t;
		m_count_base = new  my_ref_count_base<T>(t);
	}

	//拷贝构造不用考虑传值是自己的情形
	my_shared_ptr(const my_shared_ptr& other)
	{
		copy_construct_from(other);
	}

	//这种要考虑传值是自己的情形
	my_shared_ptr& operator = (const my_shared_ptr& other)
	{
		if (&other == this)
			return *this;

		copy_construct_from(other);
	}

	my_shared_ptr(const my_shared_ptr&& right)
	{
		move_construct_from(std::move(right));
	}

};

template<typename T>
class my_weak_ptr : public my_ptr_base<T>
{

};