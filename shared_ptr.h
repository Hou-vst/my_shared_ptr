#pragma once
#include<mutex>

//管理类
class my_ref_count_base
{
private:
	int m_use_count;
	int m_weak_count;
	std::mutex m_mutex;

public:
	my_ref_count_base()
		:m_use_count(1), m_weak_count(1)
	{
		
	}

	virtual ~my_ref_count_base()
	{

	}

private:
	virtual void Destroy()=0;

	virtual void Delete_this()=0;

public:
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
			Destroy();
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

template<typename T>
class my_ref_count : public my_ref_count_base
{
public:
	my_ref_count(T* ptr):my_ref_count_base()
	,m_ptr(ptr)
	{

	}

private:
	T* m_ptr;
	void Destroy()
	{
		if(m_ptr)
		{
			delete m_ptr;
		}
	}

	void Delete_this()
	{
		delete this;
	}
};

template<typename T2>
class my_shared_ptr;

template<typename T2>
class my_weak_ptr;

//这一部分已经输于智能指针类的部分了，要和管理类区分开
//智能指针类要包含管理类和指向被管理资源的指针
template<typename T>
class my_ptr_base
{

private:
	//不允许拷贝构造
	my_ptr_base(const my_ptr_base&){}
	my_ptr_base& operator=(const my_ptr_base&){}

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
	bool construct_from_weak(const my_weak_ptr<T2>& other)
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

	void Dec_Ref()
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

	void swap(my_ptr_base& right)
	{
		my_ref_count_base* tmp=right.m_count_base;
		right.m_count_base=m_count_base;
		m_count_base=tmp;

		T* tmp_ptr=right.m_ptr;
		right.m_ptr=m_ptr;
		m_ptr=tmp_ptr;
	}

	void set_ptr_countbase(T* other_ptr,my_ref_count_base* other)
	{
		m_ptr=other_ptr;
		m_count_base=other;
	}

private:
	my_ref_count_base * m_count_base;
	T* m_ptr;
};

template<typename T>
class my_shared_ptr : public my_ptr_base<T>
{
public:
	my_shared_ptr()
	{

	}

	~my_shared_ptr()
	{
		this->Dec_Ref();
	}

	my_shared_ptr(T* t)
	{
		set_ptr_countbase(t,new my_ref_count<T>(t));
	}

	//拷贝构造不用考虑传值是自己的情形
	my_shared_ptr(const my_shared_ptr& other)
	{
		copy_construct_from(other);
	}

	//这种要考虑传值是自己的情形
	my_shared_ptr& operator = (const my_shared_ptr& other)
	{
		//下面的方式是错误的，通过运算符重载处理智能指针对象的时候，除了增加新指向对象的引用计数
		//还有减少之前指向对象的引用计数，下面的方式明显忽略了这点
		//if (&other == this)
		//	return *this;

		//copy_construct_from(other);

		//通过构建临时对象的方式来管理之前的指向
		my_shared_ptr(other).swap(*this);

		return *this;
	}

	template<typename T2>
	my_shared_ptr(const my_weak_ptr<T2>& other)
	{
		this->construct_from_weak(other);
	}


	my_shared_ptr(my_shared_ptr&& right)
	{
		move_construct_from(std::move(right));
	}

	my_shared_ptr& operator=(my_shared_ptr&& right)
	{
		my_shared_ptr(std::move(right)).swap(*this);

		return *this;
	}

	void reset()
	{
		my_shared_ptr().swap(*this);
	}

	void reset(T* ptr)
	{
		my_shared_ptr(ptr).swap(*this);
	}

};

template<typename T>
class my_weak_ptr : public my_ptr_base<T>
{
public:
	my_weak_ptr()
	{

	}

	~my_weak_ptr()
	{
		this->Dec_weak_Ref();
	}

	//弱引用只能指向智能指针对象
	//my_weak_ptr(T* t)
	//{
	//
	//}

	my_weak_ptr(const my_weak_ptr& other)
	{
		Weakly_construct_from(other);
	}

	my_weak_ptr& operator = (const my_weak_ptr& other)
	{
		my_weak_ptr(other).swap(*this);
		return *this;
	}

	template<typename T2>
	my_weak_ptr(const my_shared_ptr<T2>& other)
	{
		this->Weakly_construct_from(other);
	}

	my_weak_ptr& operator = (const my_shared_ptr& other)
	{
		my_weak_ptr(other).swap(*this);

		return *this;
	}

	my_weak_ptr(my_weak_ptr&& right)
	{
		move_construct_from(std::move(right));
	}

	my_weak_ptr& operator = (my_weak_ptr&& right)
	{
		my_weak_ptr(right).swap(*this);
		return *this;
	}

	my_shared_ptr<T> lock()
	{
		my_shared_ptr<T> shared;

		shared.construct_from_weak(*this);
		return shared;
	}

	bool expired()
	{
		return this->use_count() == 0;
	}
};
