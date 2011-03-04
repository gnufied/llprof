package RRProf;
import java.io.*;
import java.net.*;
import java.nio.*;
import java.util.*;

class Monitor {

	public void hexDump(ByteBuffer buf)
	{
		for(int i = 0; i < buf.capacity(); i++)
		{
			if(i % 32 == 0)
		    {
				if(i != 0)
					System.out.printf("\n");
		       System.out.printf("%8X: ", i);
		    }
			System.out.printf("%2X ", buf.get(i));
		}
		System.out.printf("\n");
	}

	String server;
	int port;
	Socket sock;
	DataInputStream input;
	DataOutputStream output;
	long inputSize, outputSize;
	
    DataStore dataStore;
	SlideInfo slide_info;
	HashMap<Long, String> method_name_map;
	HashMap<Long, String> class_name_map;
	ProfileData currentProfileData;

    long allTime;
    long allCount;

	Monitor()
	{
        dataStore = new DataStore();
		method_name_map = new HashMap<Long, String>();
		class_name_map = new HashMap<Long, String>();
		dataStore.setNameMap(method_name_map, class_name_map);
		allTime = 0;
		allCount = 0;
		inputSize = 0;
		outputSize = 0;
	}
	
	public long getInputSize() {
		return inputSize;
	}
	
	public long getOutputSize() {
		return outputSize;
	}
	
	public void send(ByteBuffer buf) throws IOException
	{
		output.write(buf.array());
		outputSize += buf.capacity();
		/*
		System.out.print("\n[Buffer]\n");
		int cnt = 0;
		for(byte b: buf.array())
		{
			cnt++;
			System.out.printf("%X ", b);
			if((cnt % 32) == 0)
			{
				System.out.print("\n");				
			}
		}
		*/
	}

	public ByteBuffer recv(int len) throws IOException
	{
		ByteBuffer buf = ByteBuffer.allocate(len);
		int offset = 0;
		while(true)
		{
			int readsz = input.read(buf.array(), offset, len-offset);
			offset += readsz;
			if(offset >= len)
				break;
		}
		inputSize += buf.capacity();
		return buf;
	}
	
	public boolean is_connected()
	{
		return this.sock != null;
	}
	
    public boolean startProfile(String server, int port)
    {
		this.port = port;
		this.server = server;
		try{
			this.sock = new Socket(server, port);
			this.input = new DataInputStream(sock.getInputStream());
			this.output = new DataOutputStream(sock.getOutputStream());
			StartProfile msg = new StartProfile();
			msg.setProfileType(StartProfile.CALL_TREE);
			sendRequestAndReceiveResponse(msg);
			
			QueryInfo ri_msg = new QueryInfo();
			ri_msg.setInfoType(QueryInfo.INFO_DATA_SLIDE);
			SlideInfo slide_info = (SlideInfo)sendRequestAndReceiveResponse(ri_msg);
			this.slide_info = slide_info;
			slide_info.printSlideInfo();
		}
		catch(IOException e){
			this.sock = null;
			return false;
		}
        
        return true;
    }

    public MessageBase recvResponse() throws IOException
    {
        ByteBuffer recvbuf = recv(8);
        recvbuf.order(ByteOrder.LITTLE_ENDIAN);
        int msgid = recvbuf.getInt();
        int msgsz = recvbuf.getInt();
        
        ByteBuffer msg_recvbuf = recv(msgsz);
        msg_recvbuf.order(ByteOrder.LITTLE_ENDIAN);
       
        MessageBase recvmsg = MessageBase.fromBuffer(msgid, msg_recvbuf);
        return recvmsg;
    }
    public void sendRequest(MessageBase msg) throws IOException
    {
		ByteBuffer buf = msg.toBuffer();
		send(buf);
    }

    public MessageBase sendRequestAndReceiveResponse(MessageBase msg) throws IOException
    {
    	sendRequest(msg);
    	return recvResponse();
    }
    
    public ProfileData getProfileData() throws IOException
    {
    	ProfileData ret = null;
    	ArrayList<QueryNames> query_names_list = null;
    	HashMap<Long, ProfileData> profiledata_all = new HashMap<Long, ProfileData>();
    	sendRequest(new ReqProfileData());
		// System.out.println("getProfileData");
    	while(true)
    	{
    		MessageBase rmsg = recvResponse();
    		if(rmsg.isFinMessage())
    			break;
    		// System.out.println("Message: " + Long.toString(rmsg.getMessageType()));
    		if(rmsg.getMessageType() == MessageTypes.MSG_PROFILE_DATA)
    		{
    			ProfileData pdata = (ProfileData)rmsg;
        		// System.out.println("TargetThread: " + Long.toString(pdata.getTargetThreadID()));
    			ret = pdata;
    			pdata.setSlideInfo(slide_info);
		       
		       QueryNames current_query = null;
		       for(int i = 0; i < pdata.numRecords(); i++)
		        {
		        	long cls = pdata.getLong(i, "class");
		        	long mid = pdata.getLong(i, "mid");
		        	
		        	if(!method_name_map.containsKey(mid) || !class_name_map.containsKey(cls))
		        	{
		        		if(query_names_list == null)
		        		{
		        			query_names_list = new ArrayList<QueryNames>();
		        		}
		        		if(current_query == null)
		        		{
		        			current_query = new QueryNames();
		        			query_names_list.add(current_query);
		        		}
		        		if(!method_name_map.containsKey(mid))
		        		{
		        			method_name_map.put(mid, "<Asking>");
		        			current_query.addMethodID(mid);
		        		}
		        		if(!class_name_map.containsKey(cls))
		        		{
		        			class_name_map.put(cls, "<Asking>");
		        			current_query.addClassID(cls);
		        		}
		        		
		        		if(current_query.size() > 1000)
		        		{
		        			current_query = null;
		        		}
		        	}
		        	//DEBUG
		        	// System.out.println("GetInfo: p=" + Long.toString(pdata.getInt(i, "parent_node_id")) + " " + class_name_map.get(cls) + "::" + method_name_map.get(mid));
		        }
		       profiledata_all.put(pdata.getTargetThreadID(), pdata);
    		}
    		else if(rmsg.getMessageType() == MessageTypes.MSG_STACK_DATA)
    		{
    			StackData sdata = (StackData)rmsg;
    			sdata.setSlideInfo(slide_info);

    			assert profiledata_all.containsKey(sdata.getTargetThreadID());
    			profiledata_all.get(sdata.getTargetThreadID()).setStackData(sdata);
    		}
    	}
    	if(ret == null)
    		return null;
       if(query_names_list != null)
       {
    	   for(QueryNames q: query_names_list)
    	   {
	    	   MessageBase msg = sendRequestAndReceiveResponse(q);
	    	   Names names = (Names)msg;
	    	   names.setQueryInfo(q);
	    	   names.mergeMethodTo(method_name_map);
	    	   names.mergeClassTo(class_name_map);
    	   }
       }
       currentProfileData = null;
       for(Map.Entry<Long, ProfileData> entry: profiledata_all.entrySet()) {
           dataStore.recordData(entry.getValue());
       }
    	return ret;
    }
    public String getMethodName(long mid)
    {
    	return method_name_map.get(mid);
    }

    public void printProfileData()
    {
		System.out.println("numRecords: " + currentProfileData.numRecords());
		
		System.out.println(
				"-----------------------------------------------------------------------------------------"
			);
		System.out.printf(
				"%10s %24s %12s %12s %12s",
				"class", "method", "all", "children", "count" 
		);
	
		System.out.println();
		
		for(int i = currentProfileData.numRecords() - 20; i < currentProfileData.numRecords(); i++)
		{
			if(i < 0) continue;
			System.out.printf(
						"%10x %24s %12d %12d %12d",
						currentProfileData.getLong(i, "class"),
						getMethodName(currentProfileData.getLong(i, "mid")),
						currentProfileData.getLong(i, "all_time"),
						currentProfileData.getLong(i, "children_time"),
						currentProfileData.getLong(i, "call_count")
					);

	    	System.out.println();
		}
		currentProfileData.printStackData();
    }
    public void checkData()
    {

		for(int i = 0; i < currentProfileData.numRecords(); i++)
		{
			allTime += currentProfileData.getLong(i, "all_time");
			allCount += currentProfileData.getLong(i, "call_count");
		}
    	System.out.printf("Call-Count:%d  Time:%d\n", allCount, allTime);
    }
    
    public DataStore getDataStore()
    {
        return dataStore;
    }

}
