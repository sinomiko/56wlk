/***************************************************************************
 * 
 * 
 **************************************************************************/
 
 
 
/**
 * @file bsl_readmap.h
 * @brief A read-only hash table
 *  
 **/


#ifndef  __BSL_READMAP_H_
#define  __BSL_READMAP_H_

#include "bsl/alloc/bsl_alloc.h"
#include "bsl/utils/bsl_utils.h"
#include "bsl/containers/hash/bsl_hashutils.h"
#include "bsl/archive/bsl_serialization.h"
#include <string.h>
#include <algorithm>

namespace bsl{

template <class _Key, class _Value,
		 class _HashFunction = xhash<_Key>,
		 class _EqualFunction = std::equal_to<_Key>,
		 class _Alloc = bsl_alloc<_Key> >
class readmap{
private:
	//data structure types
	typedef _Key key_type;
	typedef _Value value_type;
	typedef typename std::pair<_Key, _Value> pair_type;
	typedef size_t size_type;
	typedef size_t sign_type;//Hash signature type, generated by HashFunction
	typedef readmap<_Key, _Value,
		 _HashFunction, _EqualFunction, _Alloc >  self_type;

	struct _node_type{
		size_type idx, pos, hash_value;
	}; 
public:
	typedef pair_type* iterator;
	typedef _node_type node_type;
private:
	//Allocator types
	typedef typename _Alloc::template rebind <iterator> :: other itr_alloc_type;
	typedef typename _Alloc::template rebind <pair_type> :: other pair_alloc_type;
	typedef typename _Alloc::template rebind <node_type> :: other node_alloc_type;
public:
	//compare functions 
	static int _cmp_hash(node_type __a, node_type __b){ return __a.hash_value < __b.hash_value; }
	static int _cmp_idx(node_type __a, node_type __b){ return __a.idx < __b.idx; }
private:
	//data
	iterator* _bucket;
	iterator _storage;
	size_type _hash_size;		/**<哈希桶的大小        */
	size_type _size;		  /**<哈希中元素的个数        */
	size_type _mem_size;	/**< 内存空间，=_hash_size+1    */
	//Object Function
	_HashFunction _hashfun;
	_EqualFunction _eqfun;
	//Allocators
	itr_alloc_type _itr_alloc;
	pair_alloc_type _pair_alloc;
	node_alloc_type _node_alloc;
public:
	/**
	 * @brief 创建hash表，指定桶大小和hash函数、比较函数
	 *
	 * @param [in] __bucket_size   : const size_type 
	 *				桶大小
	 * @param [in] __hash_func   : const _HashFunction& 
	 *				hash函数，参数为__key&,返回值为size_t
	 * @param [in] __equal_func   : const _EqualFunction&
	 *				比较函数，默认使用==
	 * @return  int 
	 *			<li> -1：出错
	 *			<li> 0：成功
	 * @retval   
	 * @see 
	 * @note 如果hash桶已经创建，第二次调用create时会先destroy之前的hash表
	**/
	int create(const size_type __bucket_size = 65535, 
			const _HashFunction& __hash_func = _HashFunction(), 
			const _EqualFunction& __equal_func = _EqualFunction()){
		if(_hash_size)destroy();//防止重复create

		_itr_alloc.create();
		_pair_alloc.create();
		_node_alloc.create();

		_hashfun = __hash_func;
		_eqfun = __equal_func;
		_hash_size = __bucket_size;

		if(_hash_size <= 0) return -1;
		_mem_size = _hash_size + 1;//可以优化查询速度
		_bucket = _itr_alloc.allocate(_mem_size);
		if(0 == _bucket) return -1;
		if (0 < _mem_size) {
			_bucket[0] = NULL;
		}
		return 0;
	}

	template <class _PairIterator>
	/**
	 * @brief 向hash表中赋初始元素。将[start, finish)区间的元素插入hash表<br>
	 *			[start, finish)为左闭右开区间 <br>
	 *			如果assign之前未显式调用create，assign会自动调用create <br>
	 *
	 * @param [in] __start   : _PairIterator 起始位置
	 * @param [in] __finish   : _PairIterator 终止位置
	 * @return  int 
	 *			<li> 0 : 成功
	 *			<li> -1 : 其他错误
	 * @retval   
	 * @see 
	 * @note 插入的key值不可以有相同
	**/
	int assign(const _PairIterator __start, const _PairIterator __finish){
		if(_size) clear();
		_size = std::distance(__start, __finish);

		//如果未显示调用create，则自动创建，桶大小为4倍
		if(0 == _hash_size) {
			if(create(_size * 4)) return -1;
		}

		iterator p = _pair_alloc.allocate(_size);
		if(0 == p) {
			_size = 0;
			return -1;
		}
		_storage = p;

		node_type* temp = _node_alloc.allocate(_size);
		if(0 == temp) {
			_pair_alloc.deallocate(p, _size);
			_size = 0;
			return -1;
		}

		_PairIterator itr = __start;
		size_type i, j;
		for(i = 0; itr != __finish; ++itr, ++i){
			temp[i].idx = i;
			temp[i].hash_value = _hashfun(itr->first) % _hash_size;
		}

		//为每个萝卜分配一个坑
		std::sort(temp, temp + _size, _cmp_hash);
		for(i = 0; i < _size; ++i){
			temp[i].pos = i;
		}

		//计算桶入口
		for(i = 0, j = 0; j < _mem_size; ++j){
			while(i < _size && temp[i].hash_value < j)++i;
			_bucket[j] = &_storage[i];
		}

		//拷贝内存到存储区
		std::sort(temp, temp + _size, _cmp_idx);
		for(i = 0, itr = __start; i < _size; ++i, ++itr){
			bsl::bsl_construct(&_storage[temp[i].pos], *itr);
		}

		_node_alloc.deallocate(temp, _size);

		return 0;
	}

	/**
	 * @brief 根据Key值查询
	 *
	 * @param [in] __key   : const key_type& 要查询的Key值
	 * @param [out] __value   : value_type* 查询结果的Value值
	 * @return  int 
	 *			<li> HASH_EXIST : 存在
	 *			<li> HASH_NOEXIST : 不存在
	 * @retval   
	 * @see 
	**/
	int get(const key_type& __key, value_type* __value=NULL) const {
		if (NULL == _bucket[0]) {
			return HASH_NOEXIST;
		}
		sign_type hash_value = _hashfun(__key) % _hash_size;
		for(iterator p = _bucket[hash_value]; p < _bucket[hash_value + 1]; ++p){
			if(_eqfun(__key, p->first)){
				if(__value)*__value = p->second;
				return HASH_EXIST;
			}
		};
		return HASH_NOEXIST;
	}

	/**
	 * @brief 返回hash表中的元素个数
	 *
	 * @return  size_type 元素个数
	 * @retval   
	 * @see 
	**/
	size_type size() const {
		return _size;
	}

	/**
	 * @brief 返回hash表桶大小
	 *
	 * @return  size_type 桶大小
	 * @retval   
	 * @see 
	**/
	size_type bucket_size() const {
		return _hash_size;
	}

	/**
	 * @brief 销毁hash表
	 *
	 * @return  void 
	 * @retval   
	 * @see 
	**/
	void destroy(){
		clear();
		if(_mem_size > 0 && NULL != _bucket){
			_itr_alloc.deallocate(_bucket, _mem_size);
			_mem_size = _hash_size = 0;
			_bucket = NULL;
		}
		_pair_alloc.destroy();
		_itr_alloc.destroy();
		_node_alloc.destroy();
	}

	/**
	 * @brief 清空hash表中的元素
	 *
	 * @return  void 
	 * @retval   
	 * @see 
	**/
	void clear(){
		if(_size > 0 && NULL != _storage){
			bsl::bsl_destruct(_storage, _storage + _size);
			_pair_alloc.deallocate(_storage, _size);
			_size = 0;
			_storage = NULL;
		}
	}


	/**
	 * @brief 存储区的头指针。用于遍历hash表中的元素。<BR>
	 *		仅用于访问操作
	 *
	 * @return  const iterator 头部指针 对于空容器返回NULL
	 * @retval   
	 * @see 
	**/
	const iterator begin(){
		return _storage;
	}

	/**
	 * @brief 存储区的尾指针。
	 *
	 * @return  const iterator 尾部指针（指向内存单元不可以被访问） 
	 *				对于空容器返回为NULL
	 * @retval   
	 * @see 
	**/
	const iterator end(){
		return _storage + size();
	}

	readmap(){
		_bucket = NULL;
		_storage = NULL;
		_hash_size = 0;
		_mem_size = 0;
		_size = 0;
	}

	~readmap(){
		destroy();
	}

	template <class _Archive>
	/**
	 * @brief 串行化。将hash表串行化到指定的流
	 *
	 * @param [in/out] ar   : _Archive& 串行化流
	 * @return  int 
	 *			<li> 0 : 成功
	 *			<li> -1 : 失败
	 * @retval   
	 * @see 
	**/
	int serialization(_Archive & ar) {
		if(bsl::serialization(ar, _hash_size)) goto _ERR_S;
		if(bsl::serialization(ar, _size)) goto _ERR_S;
		size_type i;
		for(i = 0; i < _size; i++){
			if(bsl::serialization(ar, _storage[i]))goto _ERR_S;
		}
		return 0;
_ERR_S:
		return -1;
	}

	template <class _Archive>
	/**
	 * @brief 反串行化。从串行化流中获取数据重建hash表
	 *
	 * @param [in] ar   : _Archive& 串行化流
	 * @return  int 
     *          <li> 0 : 成功
     *          <li> -1 : 失败
	 * @retval   
	 * @see 
	**/
	int deserialization(_Archive & ar){
		size_type fsize, hashsize, i;
		iterator tmp_storage = NULL;

		destroy();

		if(bsl::deserialization(ar, hashsize)) goto _ERR_DS;
		if(bsl::deserialization(ar, fsize)) goto _ERR_DS;
		if(hashsize <= 0 || fsize <= 0) goto _ERR_DS;

		if(create(hashsize))goto _ERR_DS;

		tmp_storage = _pair_alloc.allocate(fsize);
		if(NULL == tmp_storage) goto _ERR_DS;


		for(i = 0; i < fsize; i++)
			bsl::bsl_construct(&tmp_storage[i]);

		for(i = 0; i < fsize; i++)
			if(bsl::deserialization(ar, tmp_storage[i])) goto _ERR_DS;

		if(assign(tmp_storage, tmp_storage + fsize)) goto _ERR_DS;

		bsl::bsl_destruct(tmp_storage, tmp_storage + fsize);
		_pair_alloc.deallocate(tmp_storage, fsize);
		return 0;
_ERR_DS:
		if(NULL != tmp_storage){
			bsl::bsl_destruct(tmp_storage, tmp_storage + fsize);
			_pair_alloc.deallocate(tmp_storage, fsize);
		}
		destroy();
		return -1;
	}

private:
	readmap(const self_type &self){
	}

	self_type& operator= (const self_type &self){
		return *this;
	};

};//class readmap

}//namespace bsl















#endif  //__BSL_READMAP_H_

/* vim: set ts=4 sw=4 sts=4 tw=100 noet: */
