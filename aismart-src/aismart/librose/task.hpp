#ifndef LIBROSE_TASK_HPP_INCLUDED
#define LIBROSE_TASK_HPP_INCLUDED

#include "config_cache.hpp"
#include "config.hpp"
#include "version.hpp"
#include <set>

#include <boost/bind.hpp>
#include <boost/function.hpp>

class tcallback_lock
{
public:
	tcallback_lock(bool result, const boost::function<void (const bool)>& callback)
		: result_(result)
		, did_destruction_(callback)
	{}

	~tcallback_lock()
	{
		if (did_destruction_) {
			did_destruction_(result_);
		}
	}
	void set_result(bool val) { result_ = val; }

private:
	bool result_;
	boost::function<void (const bool)> did_destruction_;
};

class ttask
{
public:
	template<class T>
	static std::unique_ptr<T> create_task(const config& cfg, const std::string& key, void* context)
	{
		T* task = new T(cfg, context);
		task->parse_cfg(cfg, key);
		return std::unique_ptr<T>(task);
	}

	static const std::string res_alias; // ex: c:/ddksample/apps-res
	static const std::string src_alias; // ex: c:/ddksample/apps-src
	static const std::string src2_alias; // ex: c:/ddksample/apps-res/apps
	static const std::string user_alias; // ex: user data directory

	static const std::string app_res_alias; // ex: c:/ddksample/blesmart-res
	static const std::string app_src_alias; // ex: c:/ddksample/blesmart-src
	static const std::string app_src2_alias; // ex: c:/ddksample/blesmart-res/blesmart


	enum res_type {res_none, res_file, res_dir, res_files};

	class tsubtask {
	public:
		static std::string type_2_printable_str(const int type);
		int str_2_type(const std::string& str);

		virtual bool handle() = 0;
		virtual bool fail_rollback() const = 0;

		const std::string& id() const { return subtask_id_; }

	protected:
		explicit tsubtask(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths)
			: task_(task)
			, subtask_id_(id)
			, paths_(base_paths)
		{}

	protected:
		void replace_str(std::string& name, const std::pair<std::string, std::string>& replace) const;
		bool do_delete_path(const std::set<std::string>& paths) const;
		bool make_path(const std::string& tag, bool del) const;
		const std::string& alias_2_path(const std::string& alias) const;

	protected:
		ttask& task_;
		const std::string subtask_id_;
		std::map<std::string, std::string>& paths_;
	};

	class tsubtask_copy: public tsubtask
	{
	public:
		explicit tsubtask_copy(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths);

		bool handle() override;
		bool fail_rollback() const override;

	private:
		void did_fail_rollback(const bool result) const;
		bool do_copy(const std::string& src_alias, const std::string& dst_alias);
		std::set<std::string> generate_full_require_delete(const std::set<std::string>& require_delete);

		struct tres {
			tres(res_type type, const std::string& name, const std::string& custom, const bool overwrite)
				: type(type)
				, name(name)
				, custom(custom)
				, overwrite(overwrite)
			{}

			res_type type;
			std::string name;
			std::string custom;
			bool overwrite;
		};
		void resolve_res_2_rollback(const res_type type, const std::string& name, const std::set<std::string>* files = NULL);

	private:
		std::string src_alias_;
		std::string dst_alias_;

		std::vector<tres> copy_res_;
		std::set<std::string> require_delete_;

		struct trollback {
			trollback(const res_type type, const std::string& name)
				: type(type)
				, name(name)
			{}

			res_type type;
			std::string name;
		};

		std::map<int, trollback> rollbacks_;
		int current_number_;
		std::set<std::string> existed_paths_;
		std::set<std::string> created_paths_;
	};

	class tsubtask_remove: public tsubtask
	{
	public:
		explicit tsubtask_remove(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths);

		bool handle() override;
		bool fail_rollback() const override { return true; }

	private:
		bool do_remove(const std::string& dst_alias) const;

	private:
		struct tres {
			tres(res_type type, const std::string& name)
				: type(type)
				, name(name)
			{}

			res_type type;
			std::string name;
		};
		std::string obj_alias_;
		std::vector<tres> remove_res_;
	};

	class tsubtask_rename: public tsubtask
	{
	public:
		explicit tsubtask_rename(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths);

		bool handle() override;
		bool fail_rollback() const override { return true; }

	private:
		bool do_rename(const std::string& dst_alias) const;

	private:
		struct tres {
			tres(res_type type, const std::string& name, const std::string& new_name)
				: type(type)
				, name(name)
				, new_name(new_name)
			{}

			res_type type;
			std::string name;
			std::string new_name;
		};
		std::string base_path_alias_;
		std::vector<tres> rename_res_;
	};

	class tsubtask_replace: public tsubtask
	{
	public:
		explicit tsubtask_replace(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths);

		bool handle() override;
		bool fail_rollback() const override { return true; }

	private:
		struct tres {
			tres(const std::string& file, const std::vector<std::pair<std::string, std::string> >& replaces)
				: file(file)
				, replaces(replaces)
			{}

			std::string file;
			std::vector<std::pair<std::string, std::string> > replaces;
		};
		std::string base_path_alias_;
		std::vector<tres> replace_res_;
	};

	ttask(const config&);

	void parse_cfg(const config& cfg, const std::string& key);
	bool handle();
	void fail_rollback();
	const std::string& alias_2_path(const std::string& alias) const;
	void set_alias(const std::string& alias, const std::string& val);

	const std::string& name() const { return name_; }
	virtual bool valid() const;
	bool make_path(const std::string& tag, bool del) const;

protected:
	void complete_paths(const config& cfg);

private:
	virtual void app_complete_paths() {}

	// @last indicate it is last subtask.
	// if return false, will not execute this subtask. but don't fail. 
	virtual bool app_can_execute(const tsubtask& subtask, const bool last) { return true; }
	// @last indicate it is last subtask.
	// if return false, think this task fail, will execute fail rollback. 
	virtual bool app_post_handle(const tsubtask& subtask, const bool last) { return true; }

	virtual bool app_filter(const tsubtask& subtask, const std::string& src, bool dir) { return true; }

	virtual std::vector<std::pair<std::string, std::string> > app_get_replace(const tsubtask& subtask) { return std::vector<std::pair<std::string, std::string> >(); }

protected:
	std::vector<std::unique_ptr<tsubtask> > subtasks_;

	std::string name_;
	std::map<std::string, std::string> paths_;
};

const std::string& alias_2_path(const std::map<std::string, std::string>& paths, const std::string& alias);

#endif