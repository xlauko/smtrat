#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <thread>

#include "Node.h"
#include "SSHConnection.h"
#include "../Settings.h"
#include "../logging.h"
#include "../BenchmarkStatus.h"

namespace benchmax {
namespace ssh {

	
/**
 * Parses a node identifier of the format `server[:port]@[numberOfCores]@user@password`
 * @param _nodeAsString
 * @return 
 */
Node getNode(const string& _nodeAsString)
{
	regex noderegex("([^:@]+)(?::([^@]+))?@([^:@]+)(?::(\\d+))?(?:@(\\d+))?(?:#(\\d+))?");
	std::smatch matches;
	if (regex_match(_nodeAsString, matches, noderegex)) {
		std::string username = matches[1];
		std::string password = matches[2];
		std::string hostname = matches[3];
		unsigned long port = 22;
		unsigned long cores = 1;
		std::size_t connections = 1;
		try {
			if (matches[4] != "") port = std::stoul(matches[4]);
			if (matches[5] != "") cores = std::stoul(matches[5]);
			if (matches[6] != "") connections = std::stoul(matches[6]);
		} catch (std::out_of_range) {
			BENCHMAX_LOG_ERROR("benchmax", "Value for port or number of cores is out of range.");
			BENCHMAX_LOG_ERROR("benchmax", "\tPort: " << matches[4]);
			BENCHMAX_LOG_ERROR("benchmax", "\tCores: " << matches[5]);
		}
		return {hostname, username, password, (unsigned short)port, cores, connections};
	} else {
		BENCHMAX_LOG_ERROR("benchmax", "Invalid format for node specification. Use the following format:");
		BENCHMAX_LOG_ERROR("benchmax", "\t<user>[:<password>]@<hostname>[:<port = 22>][@<cores = 1>][#<connections = 1>]");
		exit(1);
	}
}

class SSHScheduler {
private:
	std::vector<SSHConnection*> connections;
	std::mutex mutex;
	
	SSHConnection* get() {
		std::lock_guard<std::mutex> lock(mutex);
		while (true) {
			for (auto& c: connections) {
				if (c->jobFree()) {
					c->newJob();
					return c;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	std::string tmpDirName(const Tool* tool, const fs::path& file) const {
		return "benchmax-" + std::to_string(Settings::startTime) + "-" + std::to_string(std::size_t(tool)) + "-" + std::to_string(std::hash<std::string>()(file.native()));
	}
public:
	SSHScheduler() {
		for (const auto& s: Settings::ssh_nodes) {
			Node n = getNode(s);
			for (std::size_t i = 0; i < n.connections; i++) {
				connections.push_back(new SSHConnection(n));
			}
		}
	}
	~SSHScheduler() {
		for (auto& c: connections) delete c;
	}
	
	void uploadTool(const Tool* tool) {
		BENCHMAX_LOG_DEBUG("benchmax.ssh", "Uploading " << tool);
		std::set<std::string> nodes;
		for (SSHConnection* c: connections) {
			// Check if we have already uploaded to this host
			if (!nodes.insert(c->getNode().hostname).second) continue;
			c->uploadFile(tool->binary().native(), Settings::ssh_basedir, tool->binary().filename().native(), S_IRWXU);
		}
	}
	
	bool executeJob(const Tool* tool, const fs::path& file, Results& res) {
		SSHConnection* c = get();
		BENCHMAX_LOG_INFO("benchmax.ssh", "Executing " << file);
		// Create temporary directory
		std::string folder = c->createTmpDir(tmpDirName(tool,file));
		// Upload benchmark file
		c->uploadFile(file, folder, file.filename().native());
		// Execute benchmark run
		BenchmarkResult result;
		std::string cmdLine = tool->getCommandline(folder + file.filename().native(), Settings::ssh_basedir + tool->binary().filename().native());
		if (!c->executeCommand(cmdLine, result)) {
			BENCHMAX_LOG_ERROR("benchmax.ssh", "Failed to execute command.");
		}
		tool->additionalResults(file, result);
		// Remove temporary directory
		c->removeDir(folder);
		// Store result
		res.addResult(tool, file, result);
		c->finishJob();
		return true;
	}
};

}
}
