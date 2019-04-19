#pragma once
#include<mutex>

//������
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
			//ɾ�����������Դ
			Destroy();
			//�²⣬�����ó�ʼ��ʱ��1��Ϊ�˷�ֹ������Դ�಻�ͷţ�Ҫ�ڱ�������Դ���ͷ�ʱ�����������ü���
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

//��һ�����Ѿ���������ָ����Ĳ����ˣ�Ҫ�͹��������ֿ�
//����ָ����Ҫ�����������ָ�򱻹�����Դ��ָ��
template<typename T>
class my_ptr_base
{

private:
	//������������
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

	//��ֵ���첻�漰���ü����ı仯�����Բ���������shared_ptr����weak_ptr��ֱ�Ӳ�������
	template<class T2>
	void move_construct_from(my_ptr_base<T2>&& right)
	{
		m_ptr = right.m_ptr;
		m_count_base = right.m_count_base;

		right.m_ptr = NULL;
		right.m_count_base = NULL;
	}

	//�����ڹ���shared_ptr����
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

	//�����ڹ���shared_ptr����
	template<class T2>
	bool construct_from_weak(const my_weak_ptr<T2>& other)
	{
		//������weak_ptr����share_ptr����Ϊ��������������ڴ��ڱ������࣬���Դ���
		//�����໹���ڵ��Ǳ��������Ѿ�����Ҳ����use_countΪ0���������ʱuse_count����ʱ��Ҫ�ж����������
		if (other.m_count_base && other.m_count_base->IncrRef_nz())
		{
			m_ptr = other.m_ptr;
			m_count_base = other.m_count_base;
			return true;
		}

		return false;
	}

	//�����ڹ���weak_ptr���󣬴���Ĳ���������shared_ptrҲ������weak_ptr
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

	//�������첻�ÿ��Ǵ�ֵ���Լ�������
	my_shared_ptr(const my_shared_ptr& other)
	{
		copy_construct_from(other);
	}

	//����Ҫ���Ǵ�ֵ���Լ�������
	my_shared_ptr& operator = (const my_shared_ptr& other)
	{
		//����ķ�ʽ�Ǵ���ģ�ͨ����������ش�������ָ������ʱ�򣬳���������ָ���������ü���
		//���м���֮ǰָ���������ü���������ķ�ʽ���Ժ��������
		//if (&other == this)
		//	return *this;

		//copy_construct_from(other);

		//ͨ��������ʱ����ķ�ʽ������֮ǰ��ָ��
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

	//������ֻ��ָ������ָ�����
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
