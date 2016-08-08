/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

class StreamSocket;

class IOHookProvider : public ServiceProvider
{
 	const bool middlehook;

 public:
	enum Type
	{
		IOH_UNKNOWN,
		IOH_SSL
	};

	const Type type;

	IOHookProvider(Module* mod, const std::string& Name, Type hooktype = IOH_UNKNOWN, bool middle = false)
		: ServiceProvider(mod, Name, SERVICE_IOHOOK), middlehook(middle), type(hooktype) { }

	/** Check if the IOHook provided can appear in the non-last position of a hook chain.
	 * That is the case if and only if the IOHook instances created are subclasses of IOHookMiddle.
	 * @return True if the IOHooks provided are subclasses of IOHookMiddle
	 */
	bool IsMiddle() const { return middlehook; }

	/** Called immediately after a connection is accepted. This is intended for raw socket
	 * processing (e.g. modules which wrap the tcp connection within another library) and provides
	 * no information relating to a user record as the connection has not been assigned yet.
	 * @param sock The socket in question
	 * @param client The client IP address and port
	 * @param server The server IP address and port
	 */
	virtual void OnAccept(StreamSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) = 0;

	/** Called immediately upon connection of an outbound BufferedSocket which has been hooked
	 * by a module.
	 * @param sock The socket in question
	 */
	virtual void OnConnect(StreamSocket* sock) = 0;
};

class IOHook : public classbase
{
 public:
	/** The IOHookProvider for this hook, contains information about the hook,
	 * such as the module providing it and the hook type.
	 */
	IOHookProvider* const prov;

	IOHook(IOHookProvider* provider)
		: prov(provider) { }

	/**
	 * Called when a hooked stream has data to write, or when the socket
	 * engine returns it as writable
	 * @param sock The socket in question
	 * @param sendq Send queue to send data from
	 * @return 1 if the sendq has been completely emptied, 0 if there is
	 *  still data to send, and -1 if there was an error
	 */
	virtual int OnStreamSocketWrite(StreamSocket* sock, StreamSocket::SendQueue& sendq) = 0;

	/** Called immediately before any socket is closed. When this event is called, shutdown()
	 * has not yet been called on the socket.
	 * @param sock The socket in question
	 */
	virtual void OnStreamSocketClose(StreamSocket* sock) = 0;

	/**
	 * Called when the stream socket has data to read
	 * @param sock The socket that is ready
	 * @param recvq The receive queue that new data should be appended to
	 * @return 1 if new data has been read, 0 if no new data is ready (but the
	 *  socket is still connected), -1 if there was an error or close
	 */
	virtual int OnStreamSocketRead(StreamSocket* sock, std::string& recvq) = 0;
};

class IOHookMiddle : public IOHook
{
	/** Data already processed by the IOHook waiting to go down the chain
	 */
	StreamSocket::SendQueue sendq;

	/** Data waiting to go up the chain
	 */
	std::string precvq;

	/** Next IOHook in the chain
	 */
	IOHook* nexthook;

 protected:
	/** Get all queued up data which has not yet been passed up the hook chain
	 * @return RecvQ containing the data
	 */
	std::string& GetRecvQ() { return precvq; }

	/** Get all queued up data which is ready to go down the hook chain
	 * @return SendQueue containing all data waiting to go down the hook chain
	 */
	StreamSocket::SendQueue& GetSendQ() { return sendq; }

 public:
	/** Constructor
	 * @param provider IOHookProvider that creates this object
	 */
	IOHookMiddle(IOHookProvider* provider)
		: IOHook(provider)
		, nexthook(NULL)
	{
	}

	/** Get all queued up data which is ready to go down the hook chain
	 * @return SendQueue containing all data waiting to go down the hook chain
	 */
	const StreamSocket::SendQueue& GetSendQ() const { return sendq; }

	/** Get the next IOHook in the chain
	 * @return Next hook in the chain or NULL if this is the last hook
	 */
	IOHook* GetNextHook() const { return nexthook; }

	/** Set the next hook in the chain
	 * @param hook Hook to set as the next hook in the chain
	 */
	void SetNextHook(IOHook* hook) { nexthook = hook; }

	/** Check if a hook is capable of being the non-last hook in a hook chain and if so, cast it to an IOHookMiddle object.
	 * @param hook IOHook to check
	 * @return IOHookMiddle referring to the same hook or NULL
	 */
	static IOHookMiddle* ToMiddleHook(IOHook* hook)
	{
		if (hook->prov->IsMiddle())
			return static_cast<IOHookMiddle*>(hook);
		return NULL;
	}

	friend class StreamSocket;
};
