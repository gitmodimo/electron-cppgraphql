// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifndef TODAYMOCK_H
#define TODAYMOCK_H

#include "TodaySchema.h"

#include "AppointmentEdgeObject.h"
#include "AppointmentObject.h"
#include "FolderEdgeObject.h"
#include "FolderObject.h"
#include "MutationObject.h"
#include "NodeObject.h"
#include "PageInfoObject.h"
#include "QueryObject.h"
#include "SubscriptionObject.h"
#include "TaskEdgeObject.h"
#include "TaskObject.h"

#include <atomic>
#include <memory>
#include <stack>

namespace graphql::today {

struct RequestState : service::RequestState
{
	RequestState(size_t id)
		: requestId(id)
	{
	}

	const size_t requestId;

	size_t appointmentsRequestId = 0;
	size_t tasksRequestId = 0;
	size_t unreadCountsRequestId = 0;

	size_t loadAppointmentsCount = 0;
	size_t loadTasksCount = 0;
	size_t loadUnreadCountsCount = 0;
};

class Appointment;
class Task;
class Folder;
class Expensive;

class Query : public std::enable_shared_from_this<Query>
{
public:
	using appointmentsLoader = std::function<std::vector<std::shared_ptr<Appointment>>()>;
	using tasksLoader = std::function<std::vector<std::shared_ptr<Task>>()>;
	using unreadCountsLoader = std::function<std::vector<std::shared_ptr<Folder>>()>;

	explicit Query(appointmentsLoader&& getAppointments, tasksLoader&& getTasks,
		unreadCountsLoader&& getUnreadCounts);

	service::AwaitableObject<std::shared_ptr<object::Node>> getNode(
		service::FieldParams params, response::IdType id);
	std::future<std::shared_ptr<object::AppointmentConnection>> getAppointments(
		const service::FieldParams& params, std::optional<int> first,
		std::optional<response::Value>&& after, std::optional<int> last,
		std::optional<response::Value>&& before);
	std::future<std::shared_ptr<object::TaskConnection>> getTasks(
		const service::FieldParams& params, std::optional<int> first,
		std::optional<response::Value>&& after, std::optional<int> last,
		std::optional<response::Value>&& before);
	std::future<std::shared_ptr<object::FolderConnection>> getUnreadCounts(
		const service::FieldParams& params, std::optional<int> first,
		std::optional<response::Value>&& after, std::optional<int> last,
		std::optional<response::Value>&& before);
	std::vector<std::shared_ptr<object::Appointment>> getAppointmentsById(
		const service::FieldParams& params, const std::vector<response::IdType>& ids);
	std::vector<std::shared_ptr<object::Task>> getTasksById(
		const service::FieldParams& params, const std::vector<response::IdType>& ids);
	std::vector<std::shared_ptr<object::Folder>> getUnreadCountsById(
		const service::FieldParams& params, const std::vector<response::IdType>& ids);
	std::shared_ptr<object::NestedType> getNested(service::FieldParams&& params);
	std::vector<std::shared_ptr<object::Expensive>> getExpensive();
	TaskState getTestTaskState();
	std::vector<std::shared_ptr<object::UnionType>> getAnyType(
		const service::FieldParams& params, const std::vector<response::IdType>& ids);

private:
	std::shared_ptr<Appointment> findAppointment(
		const service::FieldParams& params, const response::IdType& id);
	std::shared_ptr<Task> findTask(const service::FieldParams& params, const response::IdType& id);
	std::shared_ptr<Folder> findUnreadCount(
		const service::FieldParams& params, const response::IdType& id);

	// Lazy load the fields in each query
	void loadAppointments(const std::shared_ptr<service::RequestState>& state);
	void loadTasks(const std::shared_ptr<service::RequestState>& state);
	void loadUnreadCounts(const std::shared_ptr<service::RequestState>& state);

	appointmentsLoader _getAppointments;
	tasksLoader _getTasks;
	unreadCountsLoader _getUnreadCounts;

	std::vector<std::shared_ptr<Appointment>> _appointments;
	std::vector<std::shared_ptr<Task>> _tasks;
	std::vector<std::shared_ptr<Folder>> _unreadCounts;
};

class PageInfo
{
public:
	explicit PageInfo(bool hasNextPage, bool hasPreviousPage)
		: _hasNextPage(hasNextPage)
		, _hasPreviousPage(hasPreviousPage)
	{
	}

	bool getHasNextPage() const noexcept
	{
		return _hasNextPage;
	}

	bool getHasPreviousPage() const noexcept
	{
		return _hasPreviousPage;
	}

private:
	const bool _hasNextPage;
	const bool _hasPreviousPage;
};

class Appointment
{
public:
	explicit Appointment(
		response::IdType&& id, std::string&& when, std::string&& subject, bool isNow);

	// EdgeConstraints accessor
	const response::IdType& id() const noexcept
	{
		return _id;
	}

	service::AwaitableScalar<response::IdType> getId() const noexcept
	{
		return _id;
	}

	std::shared_ptr<const response::Value> getWhen() const noexcept
	{
		return _when;
	}

	std::shared_ptr<const response::Value> getSubject() const noexcept
	{
		return _subject;
	}

	bool getIsNow() const noexcept
	{
		return _isNow;
	}

	std::optional<std::string> getForceError() const
	{
		throw std::runtime_error(R"ex(this error was forced)ex");
	}

private:
	response::IdType _id;
	std::shared_ptr<const response::Value> _when;
	std::shared_ptr<const response::Value> _subject;
	bool _isNow;
};

class AppointmentEdge
{
public:
	explicit AppointmentEdge(std::shared_ptr<Appointment> appointment)
		: _appointment(std::move(appointment))
	{
	}

	std::shared_ptr<object::Appointment> getNode() const noexcept
	{
		return std::make_shared<object::Appointment>(_appointment);
	}

	service::AwaitableScalar<response::Value> getCursor() const
	{
		co_return response::Value(co_await _appointment->getId());
	}

private:
	std::shared_ptr<Appointment> _appointment;
};

class AppointmentConnection
{
public:
	explicit AppointmentConnection(bool hasNextPage, bool hasPreviousPage,
		std::vector<std::shared_ptr<Appointment>> appointments)
		: _pageInfo(std::make_shared<PageInfo>(hasNextPage, hasPreviousPage))
		, _appointments(std::move(appointments))
	{
	}

	std::shared_ptr<object::PageInfo> getPageInfo() const noexcept
	{
		return std::make_shared<object::PageInfo>(_pageInfo);
	}

	std::optional<std::vector<std::shared_ptr<object::AppointmentEdge>>> getEdges() const noexcept
	{
		auto result = std::make_optional<std::vector<std::shared_ptr<object::AppointmentEdge>>>(
			_appointments.size());

		std::transform(_appointments.cbegin(),
			_appointments.cend(),
			result->begin(),
			[](const std::shared_ptr<Appointment>& node) {
				return std::make_shared<object::AppointmentEdge>(
					std::make_shared<AppointmentEdge>(node));
			});

		return result;
	}

private:
	std::shared_ptr<PageInfo> _pageInfo;
	std::vector<std::shared_ptr<Appointment>> _appointments;
};

class Task
{
public:
	explicit Task(response::IdType&& id, std::string&& title, bool isComplete);

	// EdgeConstraints accessor
	const response::IdType& id() const
	{
		return _id;
	}

	service::AwaitableScalar<response::IdType> getId() const noexcept
	{
		return _id;
	}

	std::shared_ptr<const response::Value> getTitle() const noexcept
	{
		return _title;
	}

	bool getIsComplete() const noexcept
	{
		return _isComplete;
	}

private:
	response::IdType _id;
	std::shared_ptr<const response::Value> _title;
	bool _isComplete;
	TaskState _state = TaskState::New;
};

class TaskEdge
{
public:
	explicit TaskEdge(std::shared_ptr<Task> task)
		: _task(std::move(task))
	{
	}

	std::shared_ptr<object::Task> getNode() const noexcept
	{
		return std::make_shared<object::Task>(_task);
	}

	service::AwaitableScalar<response::Value> getCursor() const noexcept
	{
		co_return response::Value(co_await _task->getId());
	}

private:
	std::shared_ptr<Task> _task;
};

class TaskConnection
{
public:
	explicit TaskConnection(
		bool hasNextPage, bool hasPreviousPage, std::vector<std::shared_ptr<Task>> tasks)
		: _pageInfo(std::make_shared<PageInfo>(hasNextPage, hasPreviousPage))
		, _tasks(std::move(tasks))
	{
	}

	std::shared_ptr<object::PageInfo> getPageInfo() const noexcept
	{
		return std::make_shared<object::PageInfo>(_pageInfo);
	}

	std::optional<std::vector<std::shared_ptr<object::TaskEdge>>> getEdges() const noexcept
	{
		auto result =
			std::make_optional<std::vector<std::shared_ptr<object::TaskEdge>>>(_tasks.size());

		std::transform(_tasks.cbegin(),
			_tasks.cend(),
			result->begin(),
			[](const std::shared_ptr<Task>& node) {
				return std::make_shared<object::TaskEdge>(std::make_shared<TaskEdge>(node));
			});

		return result;
	}

private:
	std::shared_ptr<PageInfo> _pageInfo;
	std::vector<std::shared_ptr<Task>> _tasks;
};

class Folder
{
public:
	explicit Folder(response::IdType&& id, std::string&& name, int unreadCount);

	// EdgeConstraints accessor
	const response::IdType& id() const noexcept
	{
		return _id;
	}

	service::AwaitableScalar<response::IdType> getId() const noexcept
	{
		return _id;
	}

	std::shared_ptr<const response::Value> getName() const noexcept
	{
		return _name;
	}

	int getUnreadCount() const noexcept
	{
		return _unreadCount;
	}

private:
	response::IdType _id;
	std::shared_ptr<const response::Value> _name;
	int _unreadCount;
};

class FolderEdge
{
public:
	explicit FolderEdge(std::shared_ptr<Folder> folder)
		: _folder(std::move(folder))
	{
	}

	std::shared_ptr<object::Folder> getNode() const noexcept
	{
		return std::make_shared<object::Folder>(_folder);
	}

	service::AwaitableScalar<response::Value> getCursor() const noexcept
	{
		co_return response::Value(co_await _folder->getId());
	}

private:
	std::shared_ptr<Folder> _folder;
};

class FolderConnection
{
public:
	explicit FolderConnection(
		bool hasNextPage, bool hasPreviousPage, std::vector<std::shared_ptr<Folder>> folders)
		: _pageInfo(std::make_shared<PageInfo>(hasNextPage, hasPreviousPage))
		, _folders(std::move(folders))
	{
	}

	std::shared_ptr<object::PageInfo> getPageInfo() const noexcept
	{
		return std::make_shared<object::PageInfo>(_pageInfo);
	}

	std::optional<std::vector<std::shared_ptr<object::FolderEdge>>> getEdges() const noexcept
	{
		auto result =
			std::make_optional<std::vector<std::shared_ptr<object::FolderEdge>>>(_folders.size());

		std::transform(_folders.cbegin(),
			_folders.cend(),
			result->begin(),
			[](const std::shared_ptr<Folder>& node) {
				return std::make_shared<object::FolderEdge>(std::make_shared<FolderEdge>(node));
			});

		return result;
	}

private:
	std::shared_ptr<PageInfo> _pageInfo;
	std::vector<std::shared_ptr<Folder>> _folders;
};

class CompleteTaskPayload
{
public:
	explicit CompleteTaskPayload(
		std::shared_ptr<Task> task, std::optional<std::string>&& clientMutationId)
		: _task(std::move(task))
		, _clientMutationId(std::move(clientMutationId))
	{
	}

	std::shared_ptr<object::Task> getTask() const noexcept
	{
		return std::make_shared<object::Task>(_task);
	}

	const std::optional<std::string>& getClientMutationId() const noexcept
	{
		return _clientMutationId;
	}

private:
	std::shared_ptr<Task> _task;
	std::optional<std::string> _clientMutationId;
};

class Mutation
{
public:
	using completeTaskMutation =
		std::function<std::shared_ptr<CompleteTaskPayload>(CompleteTaskInput&&)>;

	explicit Mutation(completeTaskMutation&& mutateCompleteTask);

	static double getFloat() noexcept;

	std::shared_ptr<object::CompleteTaskPayload> applyCompleteTask(
		CompleteTaskInput&& input) noexcept;
	double applySetFloat(double valueArg) noexcept;

private:
	completeTaskMutation _mutateCompleteTask;
	static std::optional<double> _setFloat;
};

class Subscription
{
public:
	explicit Subscription() = default;

	std::shared_ptr<object::Appointment> getNextAppointmentChange() const
	{
		throw std::runtime_error("Unexpected call to getNextAppointmentChange");
	}

	std::shared_ptr<object::Node> getNodeChange(const response::IdType&) const
	{
		throw std::runtime_error("Unexpected call to getNodeChange");
	}
};

class NextAppointmentChange
{
public:
	using nextAppointmentChange =
		std::function<std::shared_ptr<Appointment>(const std::shared_ptr<service::RequestState>&)>;

	explicit NextAppointmentChange(nextAppointmentChange&& changeNextAppointment)
		: _changeNextAppointment(std::move(changeNextAppointment))
	{
	}

	static size_t getCount(service::ResolverContext resolverContext)
	{
		switch (resolverContext)
		{
			case service::ResolverContext::NotifySubscribe:
				return _notifySubscribeCount;

			case service::ResolverContext::Subscription:
				return _subscriptionCount;

			case service::ResolverContext::NotifyUnsubscribe:
				return _notifyUnsubscribeCount;

			default:
				throw std::runtime_error("Unexpected ResolverContext");
		}
	}

	std::shared_ptr<object::Appointment> getNextAppointmentChange(
		const service::FieldParams& params) const
	{
		switch (params.resolverContext)
		{
			case service::ResolverContext::NotifySubscribe:
			{
				++_notifySubscribeCount;
				break;
			}

			case service::ResolverContext::Subscription:
			{
				++_subscriptionCount;
				break;
			}

			case service::ResolverContext::NotifyUnsubscribe:
			{
				++_notifyUnsubscribeCount;
				break;
			}

			default:
				throw std::runtime_error("Unexpected ResolverContext");
		}

		return std::make_shared<object::Appointment>(_changeNextAppointment(params.state));
	}

	std::shared_ptr<object::Node> getNodeChange(const response::IdType&) const
	{
		throw std::runtime_error("Unexpected call to getNodeChange");
	}

private:
	nextAppointmentChange _changeNextAppointment;

	static size_t _notifySubscribeCount;
	static size_t _subscriptionCount;
	static size_t _notifyUnsubscribeCount;
};

class NodeChange
{
public:
	using nodeChange = std::function<std::shared_ptr<object::Node>(
		const std::shared_ptr<service::RequestState>&, response::IdType&&)>;

	explicit NodeChange(nodeChange&& changeNode)
		: _changeNode(std::move(changeNode))
	{
	}

	std::shared_ptr<object::Appointment> getNextAppointmentChange() const
	{
		throw std::runtime_error("Unexpected call to getNextAppointmentChange");
	}

	std::shared_ptr<object::Node> getNodeChange(
		const service::FieldParams& params, response::IdType&& idArg) const
	{
		return _changeNode(params.state, std::move(idArg));
	}

private:
	nodeChange _changeNode;
};

struct CapturedParams
{
	// Copied in the constructor
	const service::Directives operationDirectives;
	const service::Directives fragmentDefinitionDirectives;
	const service::Directives fragmentSpreadDirectives;
	const service::Directives inlineFragmentDirectives;

	// Moved in the constructor
	const service::Directives fieldDirectives;
};

class NestedType
{
public:
	explicit NestedType(service::FieldParams&& params, int depth);

	int getDepth() const noexcept;
	std::shared_ptr<object::NestedType> getNested(service::FieldParams&& params) const noexcept;

	static std::stack<CapturedParams> getCapturedParams() noexcept;

private:
	static std::stack<CapturedParams> _capturedParams;

	// Initialized in the constructor
	const int depth;
};

class Expensive
{
public:
	static bool Reset() noexcept;

	explicit Expensive();
	~Expensive();

	std::future<int> getOrder(const service::FieldParams& params) const noexcept;

	static constexpr size_t count = 5;
	static std::mutex testMutex;

private:
	// Block async calls to getOrder until pendingExpensive == count
	static std::mutex pendingExpensiveMutex;
	static std::condition_variable pendingExpensiveCondition;
	static size_t pendingExpensive;

	// Number of instances
	static std::atomic<size_t> instances;

	// Initialized in the constructor
	const size_t order;
};

class EmptyOperations : public service::Request
{
public:
	explicit EmptyOperations();
};

} // namespace graphql::today

#endif // TODAYMOCK_H
