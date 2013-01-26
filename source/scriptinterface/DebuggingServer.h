/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEBUGGINGSERVER_H_INCLUDED
#define DEBUGGINGSERVER_H_INCLUDED

#include "third_party/mongoose/mongoose.h"
#include "ScriptInterface.h"
//#include "ThreadDebugger.h"
#include "ps/ThreadUtil.h"

#include "lib/external_libraries/libsdl.h"


enum DBGCMD { DBG_CMD_NONE=0, DBG_CMD_CONTINUE, DBG_CMD_SINGLESTEP, DBG_CMD_STEPINTO, DBG_CMD_STEPOUT };
class CBreakPoint;
class CThreadDebugger;

// TODO: What happens and what should happen to the execution of Thread B if Thread A triggered a breakpoint?
// Should the user be able to continue/single-step specific threads and leave the others in break-state?

class CDebuggingServer
{
public:
	CDebuggingServer();
	~CDebuggingServer();
	void EnableHTTP();
	
	/** @brief Register a new ScriptInerface for debugging the scripts it executes
	 *
	 * @param	name A name for the ScriptInterface
	 * @param	pScriptInterface A pointer to the ScriptInterface
	 */
	void RegisterScriptinterface(std::string name, ScriptInterface* pScriptInterface);
	
	/** @brief Unregister a ScriptInerface. Must be called before the ScriptInterface instance becomes invalid.
	 *
	 * @param	pScriptInterface A pointer to the ScriptInterface
	 */
	void UnRegisterScriptinterface(ScriptInterface* pScriptInterface);

	
	// Mongoose callback when request comes from a client 
	void* MgDebuggingServerCallback(mg_event event, struct mg_connection *conn, const struct mg_request_info *request_info);	
	
	/** @brief Aquire exclusive read and write access to the list of breakpoints.
	 *
	 * @param	breakPoints A pointer to the vector storing all breakpoints.
	 *
	 * @return  A number you need to call ReleaseBreakPointAccess();
	 *
	 * Make sure to call ReleaseBreakPointAccess after you don't need access any longer.
	 * Using this function you get exclusive access and other threads won't be able to access the breakpoints until you call ReleaseBreakPointAccess!
	 */
	double AquireBreakPointAccess(std::list<CBreakPoint>** breakPoints);
	
	/** @brief See AquireBreakPointAccess(). You must not access the pointer returend by AquireBreakPointAccess() any longer after you call this function.
	 * 	
	 * @param	breakPointsLockID The number you got when aquiring the access. It's used to make sure that this function never gets
	 *          used by the wrong thread.        
	 */	
	void ReleaseBreakPointAccess(double breakPointsLockID);
	
	/** @brief Gets the current break-positions of all CThreadDebuggers
	 *
	 * @return	Empty, if no associated CThreadDebugger object is currently in break-state (breakpoint, step etc.). 
	 *          Otherwise a JSON object containing filename (full vfs path), line number and CThreadDebugger::ID is returned per CThreadDebugger object.
	 */
	std::stringstream GetCurrentPosition();
	
	/// Called from multiple Mongoose threads and multiple ScriptInterface threads
	bool GetThreadsBreakRequested();
	bool GetSettingSimultaneousThreadBreak();
	void RequestThreadsToBreak(bool Enabled);

private:
	static const char* header400;

	/// Webserver helper function (can be called by multiple mongooser threads)
	bool GetWebArgs(struct mg_connection *conn, const struct mg_request_info* request_info, std::string argName, uint& arg);
	bool GetWebArgs(struct mg_connection *conn, const struct mg_request_info* request_info, std::string argName, std::string& arg);
	
	/// Functions that are made available via http (can be called by multiple mongoose threads)
	void GetThreadDebuggerStatus(std::stringstream& response);
	bool ToggleBreakPoint(std::string filename, uint line);
	void GetAllCallstacks(std::stringstream& response);
	void GetStackFrame(std::stringstream& response, uint nestingLevel, uint threadDebuggerID);
	bool SetNextDbgCmd(uint threadDebuggerID, DBGCMD dbgCmd);
	void SetSettingSimultaneousThreadBreak(bool Enabled);
	
	// Returns the full vfs path to all files with the extension .js found in the vfs root
	void EnumVfsJSFiles(std::stringstream& response);
	
	// Returns the content of a file. 
	void GetFile(std::string filename, std::stringstream& response);
	
	/// Shared between multiple mongoose threads
	
	// Should other threads be stopped as soon as possible after a breakpoint is triggered in a thread
	bool m_SettingSimultaneousThreadBreak;
	
	/// Shared between multiple scriptinterface threads
	uint m_LastThreadDebuggerID;
	
	/// Shared between multiple scriptinerface threads and multiple mongoose threads
	std::list<CThreadDebugger*> m_ThreadDebuggers;
	
	// The CThreadDebuggers will check this value and break the thread if it's true.
	// This only works for JS code, so C++ code will not break until it executes JS-code again.
	bool m_ThreadsBreakRequested;
	
	// The breakpoint is uniquely identified using filename an line-number.
	// Since the filename is the whole vfs path it should really be unique.
	std::list<CBreakPoint> m_BreakPoints;
	
	/// Used for controlling access to m_BreakPoints
	SDL_sem* m_BreakPointsSem;
	double m_BreakPointsLockID;
	
	/// Mutex used to protect all members of this class that need to be protected and can be protected by a mutex.
	/// We don't care about the delay that could be caused by only using one mutex at the moment.
	CMutex m_Mutex;
	
	// TODO: Currently a workaround for one possible source of a deadlock... 
	CMutex m_Mutex1;
	
	/// Not important for this class' thread-safety
	mg_context* m_MgContext;
};

extern CDebuggingServer g_DebuggingServer;

#endif // DEBUGGINGSERVER_H_INCLUDED
