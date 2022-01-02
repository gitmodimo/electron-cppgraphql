// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "TodayMock.h"

#include "AppointmentConnectionObject.h"
#include "CompleteTaskPayloadObject.h"
#include "ExpensiveObject.h"
#include "FolderConnectionObject.h"
#include "NestedTypeObject.h"
#include "TaskConnectionObject.h"
#include "UnionTypeObject.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>

namespace graphql::today {

Appointment::Appointment(
	response::IdType&& id, std::string&& when, std::string&& subject, bool isNow)
	: _id(std::move(id))
	, _when(std::make_shared<response::Value>(std::move(when)))
	, _subject(std::make_shared<response::Value>(std::move(subject)))
	, _isNow(isNow)
{
}

Task::Task(response::IdType&& id, std::string&& title, bool isComplete)
	: _id(std::move(id))
	, _title(std::make_shared<response::Value>(std::move(title)))
	, _isComplete(isComplete)
{
}

Folder::Folder(response::IdType&& id, std::string&& name, int unreadCount)
	: _id(std::move(id))
	, _name(std::make_shared<response::Value>(std::move(name)))
	, _unreadCount(unreadCount)
{
}

Query::Query(appointmentsLoader&& getAppointments, tasksLoader&& getTasks,
	unreadCountsLoader&& getUnreadCounts)
	: _getAppointments(std::move(getAppointments))
	, _getTasks(std::move(getTasks))
	, _getUnreadCounts(getUnreadCounts)
{
}

void Query::loadAppointments(const std::shared_ptr<service::RequestState>& state)
{
	if (_getAppointments)
	{
		if (state)
		{
			auto todayState = std::static_pointer_cast<RequestState>(state);

			todayState->appointmentsRequestId = todayState->requestId;
			todayState->loadAppointmentsCount++;
		}

		_appointments = _getAppointments();
		_getAppointments = nullptr;
	}
}

std::shared_ptr<Appointment> Query::findAppointment(
	const service::FieldParams& params, const response::IdType& id)
{
	loadAppointments(params.state);

	for (const auto& appointment : _appointments)
	{
		if (appointment->id() == id)
		{
			return appointment;
		}
	}

	return nullptr;
}

void Query::loadTasks(const std::shared_ptr<service::RequestState>& state)
{
	if (_getTasks)
	{
		if (state)
		{
			auto todayState = std::static_pointer_cast<RequestState>(state);

			todayState->tasksRequestId = todayState->requestId;
			todayState->loadTasksCount++;
		}

		_tasks = _getTasks();
		_getTasks = nullptr;
	}
}

std::shared_ptr<Task> Query::findTask(
	const service::FieldParams& params, const response::IdType& id)
{
	loadTasks(params.state);

	for (const auto& task : _tasks)
	{
		if (task->id() == id)
		{
			return task;
		}
	}

	return nullptr;
}

void Query::loadUnreadCounts(const std::shared_ptr<service::RequestState>& state)
{
	if (_getUnreadCounts)
	{
		if (state)
		{
			auto todayState = std::static_pointer_cast<RequestState>(state);

			todayState->unreadCountsRequestId = todayState->requestId;
			todayState->loadUnreadCountsCount++;
		}

		_unreadCounts = _getUnreadCounts();
		_getUnreadCounts = nullptr;
	}
}

std::shared_ptr<Folder> Query::findUnreadCount(
	const service::FieldParams& params, const response::IdType& id)
{
	loadUnreadCounts(params.state);

	for (const auto& folder : _unreadCounts)
	{
		if (folder->id() == id)
		{
			return folder;
		}
	}

	return nullptr;
}

template <class _Rep, class _Period>
auto operator co_await(std::chrono::duration<_Rep, _Period> delay)
{
	struct awaiter
	{
		const std::chrono::duration<_Rep, _Period> delay;

		constexpr bool await_ready() const
		{
			return true;
		}

		void await_suspend(coro::coroutine_handle<> h) noexcept
		{
			h.resume();
		}

		void await_resume()
		{
			std::this_thread::sleep_for(delay);
		}
	};

	return awaiter { delay };
}

service::AwaitableObject<std::shared_ptr<object::Node>> Query::getNode(
	service::FieldParams params, response::IdType id)
{
	// query { node(id: "ZmFrZVRhc2tJZA==") { ...on Task { title } } }
	using namespace std::literals;
	co_await 100ms;

	auto appointment = findAppointment(params, id);

	if (appointment)
	{
		co_return std::make_shared<object::Node>(
			std::make_shared<object::Appointment>(std::move(appointment)));
	}

	auto task = findTask(params, id);

	if (task)
	{
		co_return std::make_shared<object::Node>(std::make_shared<object::Task>(std::move(task)));
	}

	auto folder = findUnreadCount(params, id);

	if (folder)
	{
		co_return std::make_shared<object::Node>(
			std::make_shared<object::Folder>(std::move(folder)));
	}

	co_return nullptr;
}

template <class _Object, class _Connection>
struct EdgeConstraints
{
	using vec_type = std::vector<std::shared_ptr<_Object>>;
	using itr_type = typename vec_type::const_iterator;

	EdgeConstraints(const std::shared_ptr<service::RequestState>& state, const vec_type& objects)
		: _state(state)
		, _objects(objects)
	{
	}

	std::shared_ptr<_Connection> operator()(const std::optional<int>& first,
		std::optional<response::Value>&& after, const std::optional<int>& last,
		std::optional<response::Value>&& before) const
	{
		auto itrFirst = _objects.cbegin();
		auto itrLast = _objects.cend();

		if (after)
		{
			auto afterId = after->release<response::IdType>();
			auto itrAfter =
				std::find_if(itrFirst, itrLast, [&afterId](const std::shared_ptr<_Object>& entry) {
					return entry->id() == afterId;
				});

			if (itrAfter != itrLast)
			{
				itrFirst = itrAfter;
			}
		}

		if (before)
		{
			auto beforeId = before->release<response::IdType>();
			auto itrBefore =
				std::find_if(itrFirst, itrLast, [&beforeId](const std::shared_ptr<_Object>& entry) {
					return entry->id() == beforeId;
				});

			if (itrBefore != itrLast)
			{
				itrLast = itrBefore;
				++itrLast;
			}
		}

		if (first)
		{
			if (*first < 0)
			{
				std::ostringstream error;

				error << "Invalid argument: first value: " << *first;
				throw service::schema_exception { { service::schema_error { error.str() } } };
			}

			if (itrLast - itrFirst > *first)
			{
				itrLast = itrFirst + *first;
			}
		}

		if (last)
		{
			if (*last < 0)
			{
				std::ostringstream error;

				error << "Invalid argument: last value: " << *last;
				throw service::schema_exception { { service::schema_error { error.str() } } };
			}

			if (itrLast - itrFirst > *last)
			{
				itrFirst = itrLast - *last;
			}
		}

		std::vector<std::shared_ptr<_Object>> edges(itrLast - itrFirst);

		std::copy(itrFirst, itrLast, edges.begin());

		return std::make_shared<_Connection>(itrLast<_objects.cend(), itrFirst> _objects.cbegin(),
			std::move(edges));
	}

private:
	const std::shared_ptr<service::RequestState>& _state;
	const vec_type& _objects;
};

std::future<std::shared_ptr<object::AppointmentConnection>> Query::getAppointments(
	const service::FieldParams& params, std::optional<int> first,
	std::optional<response::Value>&& after, std::optional<int> last,
	std::optional<response::Value>&& before)
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped) {
			loadAppointments(state);

			EdgeConstraints<Appointment, AppointmentConnection> constraints(state, _appointments);
			auto connection = constraints(firstWrapped,
				std::move(afterWrapped),
				lastWrapped,
				std::move(beforeWrapped));

			return std::make_shared<object::AppointmentConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before));
}

std::future<std::shared_ptr<object::TaskConnection>> Query::getTasks(
	const service::FieldParams& params, std::optional<int> first,
	std::optional<response::Value>&& after, std::optional<int> last,
	std::optional<response::Value>&& before)
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped) {
			loadTasks(state);

			EdgeConstraints<Task, TaskConnection> constraints(state, _tasks);
			auto connection = constraints(firstWrapped,
				std::move(afterWrapped),
				lastWrapped,
				std::move(beforeWrapped));

			return std::make_shared<object::TaskConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before));
}

std::future<std::shared_ptr<object::FolderConnection>> Query::getUnreadCounts(
	const service::FieldParams& params, std::optional<int> first,
	std::optional<response::Value>&& after, std::optional<int> last,
	std::optional<response::Value>&& before)
{
	auto spThis = shared_from_this();
	auto state = params.state;
	return std::async(
		std::launch::async,
		[this, spThis, state](std::optional<int>&& firstWrapped,
			std::optional<response::Value>&& afterWrapped,
			std::optional<int>&& lastWrapped,
			std::optional<response::Value>&& beforeWrapped) {
			loadUnreadCounts(state);

			EdgeConstraints<Folder, FolderConnection> constraints(state, _unreadCounts);
			auto connection = constraints(firstWrapped,
				std::move(afterWrapped),
				lastWrapped,
				std::move(beforeWrapped));

			return std::make_shared<object::FolderConnection>(connection);
		},
		std::move(first),
		std::move(after),
		std::move(last),
		std::move(before));
}

std::vector<std::shared_ptr<object::Appointment>> Query::getAppointmentsById(
	const service::FieldParams& params, const std::vector<response::IdType>& ids)
{
	std::vector<std::shared_ptr<object::Appointment>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::make_shared<object::Appointment>(findAppointment(params, id));
		});

	return result;
}

std::vector<std::shared_ptr<object::Task>> Query::getTasksById(
	const service::FieldParams& params, const std::vector<response::IdType>& ids)
{
	std::vector<std::shared_ptr<object::Task>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::make_shared<object::Task>(findTask(params, id));
		});

	return result;
}

std::vector<std::shared_ptr<object::Folder>> Query::getUnreadCountsById(
	const service::FieldParams& params, const std::vector<response::IdType>& ids)
{
	std::vector<std::shared_ptr<object::Folder>> result(ids.size());

	std::transform(ids.cbegin(),
		ids.cend(),
		result.begin(),
		[this, &params](const response::IdType& id) {
			return std::make_shared<object::Folder>(findUnreadCount(params, id));
		});

	return result;
}

std::shared_ptr<object::NestedType> Query::getNested(service::FieldParams&& params)
{
	return std::make_shared<object::NestedType>(std::make_shared<NestedType>(std::move(params), 1));
}

std::vector<std::shared_ptr<object::Expensive>> Query::getExpensive()
{
	std::vector<std::shared_ptr<object::Expensive>> result(Expensive::count);

	for (auto& entry : result)
	{
		entry = std::make_shared<object::Expensive>(std::make_shared<Expensive>());
	}

	return result;
}

TaskState Query::getTestTaskState()
{
	return TaskState::Unassigned;
}

std::vector<std::shared_ptr<object::UnionType>> Query::getAnyType(
	const service::FieldParams& params, const std::vector<response::IdType>&)
{
	loadAppointments(params.state);

	std::vector<std::shared_ptr<object::UnionType>> result(_appointments.size());

	std::transform(_appointments.cbegin(),
		_appointments.cend(),
		result.begin(),
		[](const auto& appointment) noexcept {
			return std::make_shared<object::UnionType>(
				std::make_shared<object::Appointment>(appointment));
		});

	return result;
}

Mutation::Mutation(completeTaskMutation&& mutateCompleteTask)
	: _mutateCompleteTask(std::move(mutateCompleteTask))
{
}

std::shared_ptr<object::CompleteTaskPayload> Mutation::applyCompleteTask(
	CompleteTaskInput&& input) noexcept
{
	return std::make_shared<object::CompleteTaskPayload>(_mutateCompleteTask(std::move(input)));
}

std::optional<double> Mutation::_setFloat = std::nullopt;

double Mutation::getFloat() noexcept
{
	return *_setFloat;
}

double Mutation::applySetFloat(double valueArg) noexcept
{
	_setFloat = std::make_optional(valueArg);
	return valueArg;
}

std::stack<CapturedParams> NestedType::_capturedParams;

NestedType::NestedType(service::FieldParams&& params, int depth)
	: depth(depth)
{
	_capturedParams.push({ { params.operationDirectives },
		params.fragmentDefinitionDirectives->empty()
			? service::Directives {}
			: service::Directives { params.fragmentDefinitionDirectives->front().get() },
		params.fragmentSpreadDirectives->empty()
			? service::Directives {}
			: service::Directives { params.fragmentSpreadDirectives->front() },
		params.inlineFragmentDirectives->empty()
			? service::Directives {}
			: service::Directives { params.inlineFragmentDirectives->front() },
		std::move(params.fieldDirectives) });
}

int NestedType::getDepth() const noexcept
{
	return depth;
}

std::shared_ptr<object::NestedType> NestedType::getNested(
	service::FieldParams&& params) const noexcept
{
	return std::make_shared<object::NestedType>(
		std::make_shared<NestedType>(std::move(params), depth + 1));
}

std::stack<CapturedParams> NestedType::getCapturedParams() noexcept
{
	auto result = std::move(_capturedParams);

	return result;
}

std::mutex Expensive::testMutex {};
std::mutex Expensive::pendingExpensiveMutex {};
std::condition_variable Expensive::pendingExpensiveCondition {};
size_t Expensive::pendingExpensive = 0;

std::atomic<size_t> Expensive::instances = 0;

bool Expensive::Reset() noexcept
{
	std::unique_lock pendingExpensiveLock(pendingExpensiveMutex);

	pendingExpensive = 0;
	pendingExpensiveLock.unlock();

	return instances == 0;
}

Expensive::Expensive()
	: order(++instances)
{
}

Expensive::~Expensive()
{
	--instances;
}

std::future<int> Expensive::getOrder(const service::FieldParams& params) const noexcept
{
	return std::async(
		params.launch.await_ready() ? std::launch::deferred : std::launch::async,
		[](bool blockAsync, int instanceOrder) noexcept {
			if (blockAsync)
			{
				// Block all of the Expensive objects in async mode until the count is reached.
				std::unique_lock pendingExpensiveLock(pendingExpensiveMutex);

				if (++pendingExpensive < count)
				{
					pendingExpensiveCondition.wait(pendingExpensiveLock, []() {
						return pendingExpensive == count;
					});
				}

				// Wake up the next Expensive object.
				pendingExpensiveLock.unlock();
				pendingExpensiveCondition.notify_one();
			}

			return instanceOrder;
		},
		!params.launch.await_ready(),
		static_cast<int>(order));
}

EmptyOperations::EmptyOperations()
	: service::Request({}, GetSchema())
{
}

} // namespace graphql::today
