#pragma once

#include <functional>
#include <utility>
#include <vector>
#include <iostream>
#include <type_traits>
namespace Delegate {
	template<typename ..._Args>
	class Delegate
	{

	public:
		using Function = std::function<void(_Args...)>;

	public:
		inline void Push(Function&& _func) {

			m_functionList.emplace_back(move(_func));
		}
		inline void Pop(Function&& _func) {
			for (int idx = 0; idx < m_functionList.size(); ++idx) {
				if (m_functionList[idx].target_type() == _func.target_type()) {
					m_functionList.erase(std::next(m_functionList.begin(), idx));
					break;
				}
			}
		}
		inline void Reset() {
			m_functionList.clear();
		}


	public:
		inline void operator+=(Function& _func) {
			m_functionList.push_back(_func);
		}
		inline void operator+=(Function&& _func) {
			m_functionList.push_back(_func);
		}
		inline void operator-=(Function& _func) {
			for (int idx = 0; idx < m_functionList.size(); ++idx) {
				if (m_functionList[idx].target_type() == _func.target_type()) {
					m_functionList.erase(std::next(m_functionList.begin(), idx));
					break;
				}
			}
		}
		inline void operator-=(Function&& _func) {
			for (int idx = 0; idx < m_functionList.size(); ++idx) {
				if (m_functionList[idx].target_type() == _func.target_type()) {
					m_functionList.erase(std::next(m_functionList.begin(), idx));
					break;
				}
			}
		}
		inline void operator =(const Function& _func) {
			m_functionList.clear();
			m_functionList.push_back(_func);
		}
		/*inline void operator()(_Args&&... _types) {
			for (auto& func : m_functionList) {
				func(std::forward<_Args>(_types)...);
			}
		}*/
		// 기존 operator() 함수를 다음과 같이 수정
		inline void operator()(_Args... _types) {  // && 제거
			for (auto& func : m_functionList) {
				func(_types...);  // std::forward 제거
			}
		}

		// IsBound() 함수 추가
		inline bool IsBound() const {
			return !m_functionList.empty();
		}

		// 등록된 함수 개수 반환
		inline size_t GetFunctionCount() const {
			return m_functionList.size();
		}

	private:
		std::vector<Function> m_functionList;
	};


	//SFINAE 기법

	//Bind Function Template
	template<typename B, typename T>
	using Base_Check = typename std::enable_if<std::is_base_of<B, T>::value, bool>::type;


	// Override 된 함수가 없는 경우 해당 부모의 함수 호출하는 예외적인 함수.
	// 상속 관계에서 Overriding 체크할 경우 예외가 나오기에 추가.
	template<typename R, typename T, typename B, typename... Args, Base_Check<T, B> = NULL>
	constexpr auto Bind(R(T::* f)(Args...), B* p) {
		return [p, f](Args... args) ->R { return (static_cast<T*>(p)->*f)(args...); };
	};

	// 기본적인 Member Function을 Lambda Function으로 묶어주는 함수.
	template<typename R, typename T, typename... Args>
	constexpr auto Bind(R(T::* f)(Args...), T* p)
	{
		return [p, f](Args... args) ->R { return (p->*f)(args...); };
	};

	// 기본적인 Static Function을 Lambda Function으로 묶어주는 함수.
	template<typename R, typename... Args>
	constexpr auto Bind(R(*f)(Args...))
	{
		return [f](Args... args) ->R { return (*f)(args...); };
	};
}

