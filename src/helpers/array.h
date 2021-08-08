#define MAX_SIZE  512

template <typename T>
class Array
{
public:

	inline Array() {
		this->value = new T[this->_capacity];
	}
	inline Array(size_t n) {
		this->_capacity = n << 2;
		this->value = new T[this->_capacity];
		for (size_t i = 0; i < n; ++i)
			this->value[i] = T();
		this->count = n;
	}
	inline Array(const Array<T>& val) {
		this->_capacity = val._capacity;
		this->value = new T[this->_capacity];
		for (size_t i = 0; i < val.size(); ++i)
			this->value[i] = val.value[i];
		this->count = val.size();
	}
	inline ~Array() {
		delete[] this->value;
	}

	inline  T* data() {
		return this->value;
	}
	inline const T* data() const {
		return this->value;
	}

	inline bool   empty() const {
		return (this->count == 0);
	}
	inline size_t size() const {
		return this->count;
	}
	inline size_t max_size() const {
		return MAX_SIZE;
	}
	inline size_t capacity() const {
		return this->_capacity;
	}

	inline void   push_back(const T& val) {
		if (this->count == this->_capacity) {
			this->_capacity <<= 2;
			reallocate();
		}
		this->value[this->count] = val;
		++this->count;
	}
	inline void   pop_back() {
		--this->count;
		this->value[this->count].~T();
	}

	inline void   clear() {
		for (size_t i = 0; i < this->count; ++i)
			this->value[i].~T();
		this->count = 0;
	}
	inline void   resize(size_t val) {
		if (val > this->count) {
			if (val > this->_capacity) {
				this->_capacity = val;
				reallocate();
			}
		}
		else {
			for (size_t i = this->count; i < val; ++i)
				this->value[i].~T();
		}
		this->count = val;
	}
	inline void   reserve(size_t val) {
		if (val > this->_capacity) {
			this->_capacity = val;
			reallocate();
		}
	}
	inline void   swap(Array<T>& val) {
		size_t tmp_count = this->count;
		size_t tmp_capacity = this->_capacity;
		T* tmp_value = this->value;

		this->count = val.count;
		this->_capacity = val._capacity;
		this->value = val.value;

		val.count = tmp_count;
		val._capacity = tmp_capacity;
		val.value = tmp_value;
	}

	inline T& operator [](size_t i) {
		return this->value[i];
	}
	inline T& at(size_t i) {
		return this->value[i];
	}
	inline T& front() {
		return this->value[0];
	}
	inline T& back() {
		return this->value[this->count - 1];
	}

	inline const T& operator [](size_t i) const {
		return this->value[i];
	}
	inline const T& at(size_t i) const {
		return this->value[i];
	}
	inline const T& front() const {
		return this->value[0];
	}
	inline const T& back() const {
		return this->value[this->count - 1];
	}

	inline  T* begin() {
		return this->value;
	}
	inline  T* end() {
		return (this->value + this->count);
	}
	inline  const T* begin() const {
		return this->value;
	}
	inline  const T* end() const {
		return (this->value + this->count);
	}
private:
	T* value;
	size_t  count;
	size_t _capacity = 4;

	inline void reallocate() {
		T* tmp = new T[this->_capacity];
		memcpy(tmp, this->value, this->count * sizeof(T));
		delete[] this->value;
		this->value = tmp;
	}
	inline void copy(const Array<T>& val) {
		this->count = val.count;
		this->capacity = val.capacity;
		this->value = new T[val.capacity];
		for (size_t i = 0; i < this->count; i++)
			this->value[i] = val.value[i];
	}
};