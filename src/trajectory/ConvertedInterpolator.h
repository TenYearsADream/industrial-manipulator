/**
 * @brief ConvertedInterpolator类
 * @date: Sep 7, 2017
 * @author a1994846931931
 */

#ifndef CONVERTEDINTERPOLATOR_H_
#define CONVERTEDINTERPOLATOR_H_

# include "Interpolator.h"
# include <vector>
# include "../math/Q.h"
# include "../common/printAdvance.h"
# include <memory>
# include "../ik/IKSolver.h"
# include "../model/Config.h"
# include "../common/printAdvance.h"
# include <functional>
# include <algorithm>

using std::vector;

namespace robot {
namespace trajectory {

/**
 * @addtogroup trajectory
 * @{
 */

/**
 * @brief 转换类型的插补器模板类
 *
 * 可以将类型B构造成输出类型为T的插补器, 类型B默认是一种插补器. 在默认情况下,
 * 尝试将B插补器的输出强制类型转换成T类的输出. 类型B也可以是别的种类, 例如一个元素为
 * 插补器指针的vector, 甚至是一个常量.
 */
template <class B, class T>
class ConvertedInterpolator: public Interpolator<T> {
public:
	using ptr = std::shared_ptr<ConvertedInterpolator<B, T> >;
	/**
	 * @brief 构造函数
	 *
	 * 记录源插补器的指针
	 * @param origin [in] 源插补器
	 * @param transformx [in] 用于转变x(t)结果的函数
	 * @param transformdx [in]  用于转变dx(t)结果的函数
	 * @param transformddx [in]  用于转变ddx(t)结果的函数
	 */
	ConvertedInterpolator(std::shared_ptr<Interpolator<B> > origin, std::function<T(B)> transformx, std::function<T(B)> transformdx, std::function<T(B)> transformddx)
	:_transformx(transformx), _transformdx(transformdx), _transformddx(transformddx)
	{
		_OriginalInterpolator = origin;
	}

	T x(double t) const
	{
		return _transformx(_OriginalInterpolator->x(t));
	}

	T dx(double t) const
	{
		return _transformdx(_OriginalInterpolator->dx(t));
	}

	T ddx(double t) const
	{
		return _transformddx(_OriginalInterpolator->ddx(t));
	}

	double duration() const
	{
		return _OriginalInterpolator->duration();
	}
	virtual ~ConvertedInterpolator(){}
private:
	/** @brief 源插补器 */
	std::shared_ptr<Interpolator<B> > _OriginalInterpolator;
	std::function<T(B)> _transformx;
	std::function<T(B)> _transformdx;
	std::function<T(B)> _transformddx;
};

/**
 * @brief 转换自double类型插补器指针容器, 转换至Q
 */
template<>
class ConvertedInterpolator<std::vector<Interpolator<double>::ptr > , robot::math::Q>: public Interpolator<robot::math::Q> {
public:
	using ptr = std::shared_ptr<ConvertedInterpolator<std::vector<Interpolator<double>::ptr > , robot::math::Q> >;
	/**
	 * @brief 构造函数
	 * @param origin [in] 用于构造Q插补器的double插补器集合
	 */
	ConvertedInterpolator(std::vector<Interpolator<double>::ptr > origin)
	{
		_interpolatorList.assign(origin.begin(), origin.end());
		_size = _interpolatorList.size();
		if (_size <= 0)
			throw ("错误: 用于构造Q类型的插补器容器大小不能为0!");
	}

	robot::math::Q x(double t) const
	{
		robot::math::Q q = robot::math::Q::zero(_size);
		for (int i=0; i<_size; i++)
			q(i) = _interpolatorList[i]->x(t);
		return q;
	}

	robot::math::Q dx(double t) const
	{
		robot::math::Q q = robot::math::Q::zero(_size);
		for (int i=0; i<_size; i++)
			q(i) = _interpolatorList[i]->dx(t);
		return q;
	}

	robot::math::Q ddx(double t) const
	{
		robot::math::Q q = robot::math::Q::zero(_size);
		for (int i=0; i<_size; i++)
			q(i) = _interpolatorList[i]->ddx(t);
		return q;
	}

	double duration() const
	{
		return _interpolatorList[0]->duration();
	}
	virtual ~ConvertedInterpolator(){}
private:
	/** @brief 源插补器列表 */
	std::vector<Interpolator<double>::ptr > _interpolatorList;

	/** @brief 插补器列表大小 */
	int _size;
};

/**
 * @brief 转换位姿插补器容器到Q(使用ikSolver)
 */
template<>
class ConvertedInterpolator<std::pair<Interpolator<Vector3D<double> >::ptr , Interpolator<Rotation3D<double> >::ptr > , robot::math::Q>: public Interpolator<robot::math::Q> {
public:
	using ptr = std::shared_ptr<ConvertedInterpolator<std::pair<Interpolator<Vector3D<double> >::ptr , Interpolator<Rotation3D<double> >::ptr > , robot::math::Q> >;
	/**
	 * @brief 构造函数
	 * @param origin [in] 位姿插补器, 由一个Vector3D<double>类型的插补器和Rotation3D<double>类型的插补器构成
	 * @param iksolver [in] 用于逆解的逆解器
	 * @param config [in] 用于逆解的位姿参数
	 */
	ConvertedInterpolator(std::pair<Interpolator<Vector3D<double> >::ptr , Interpolator<Rotation3D<double> >::ptr >  origin,
			std::shared_ptr<robot::ik::IKSolver> iksolver,
			robot::model::Config config):_config(config)
	{
		_ikSolver = iksolver;
		_posInterpolator = origin.first;
		_rotInterpolator = origin.second;
		if (fabs(_posInterpolator->duration() - _rotInterpolator->duration()) > 0.001)
			common::println("警告<ikInterpolator>: 位置插补器与姿态插补器的周期不同!");
	}

	virtual robot::math::Q x(double t) const
	{
		try{
			std::vector<robot::math::Q> result = (_ikSolver->solve(HTransform3D<double>(_posInterpolator->x(t), _rotInterpolator->x(t)), _config));
			if ((int)result.size() <= 0)
			{
				robot::common::println("无法逆解的末端位姿: ");
				HTransform3D<double>(_posInterpolator->x(t), _rotInterpolator->x(t)).print();
				throw (std::string("错误<ikInterpolator>: 无法进行逆解!"));
			}
			return result[0];
		}
		catch(std::string& msg)
		{
			throw(std::string("错误<ikInterpolator>: 无法进行逆解!\n") + msg);
		}
	}

	/** @todo 如何处理 */
	virtual robot::math::Q dx(double t) const
	{
		return (this->x(t + 0.0001) - this->x(t))*10000.0;
	}

	virtual robot::math::Q ddx(double t) const
	{
		return ((this->x(t + 0.0002)) - (this->x(t + 0.0001))*2.0 +( this->x(t)))*100000000.0;
	}

	double duration() const
	{
		return _posInterpolator->duration();
	}

	virtual ~ConvertedInterpolator(){}
protected:
	/**> 逆解器 */
	std::shared_ptr<robot::ik::IKSolver> _ikSolver;

	/**> 位置插补器 */
	Interpolator<Vector3D<double> >::ptr _posInterpolator;

	/**> 姿态插补器 */
	Interpolator<Rotation3D<double> >::ptr _rotInterpolator;

	/**> 机器人姿态 - 用于逆解取解 */
	robot::model::Config _config;
};

using ikInterpolator = ConvertedInterpolator<std::pair<Interpolator<Vector3D<double> >::ptr , Interpolator<Rotation3D<double> >::ptr > , robot::math::Q>;

/** @} */
} /* namespace trajectory */
} /* namespace robot */

#endif /* CONVERTEDINTERPOLATOR_H_ */
