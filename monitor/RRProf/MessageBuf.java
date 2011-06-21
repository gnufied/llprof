package RRProf;

import java.util.*;
import java.io.*;
import java.nio.*;
import java.nio.charset.*;

class MessageTypes {
	public final static int MSG_START_PROFILE = 2;
	public final static int MSG_QUERY_INFO = 3;
	public final static int INFO_DATA_SLIDE = 1;
	public final static int INFO_PROFILE_TARGET = 2;

	public final static int MSG_REQ_PROFILE_DATA = 4;
	public final static int MSG_SLIDE_INFO = 5;
	public final static int MSG_PROFILE_DATA = 6;
	public final static int MSG_QUERY_NAMES = 7;
	public final static int MSG_NAMES = 8;
	public final static int MSG_STACK_DATA = 9;
	public final static int MSG_PROFILE_TARGET = 10;

	public final static int MSG_COMMAND_SUCCEED = 100;
	public final static int MSG_ERROR = 101;

	public final static int QUERY_NAMES = 1;

}

class MessageBase {
	int messageType;

	protected ByteBuffer newBuffer(int size) {
		ByteBuffer buf = ByteBuffer.allocate(size);
		buf.order(ByteOrder.LITTLE_ENDIAN);
		return buf;
	}

	public int getMessageType() {
		return messageType;
	}

	public boolean isFinMessage() {
		return messageType == MessageTypes.MSG_COMMAND_SUCCEED
				|| messageType == MessageTypes.MSG_ERROR;
	}

	public ByteBuffer toBuffer() {
		int body_sz = getMessageBodySize();
		ByteBuffer buf = newBuffer(body_sz + 8);
		buf.putInt(getMessageType());
		buf.putInt(body_sz);
		putMessageBody(buf);
		return buf;
	}

	public int getMessageBodySize() {
		return 0;
	}

	public void putMessageBody(ByteBuffer buf) {
		return;
	}

	public static MessageBase fromBuffer(int msgid, ByteBuffer buf) {
		switch (msgid) {
		case MessageTypes.MSG_SLIDE_INFO:
			return new SlideInfo(buf);
		case MessageTypes.MSG_PROFILE_DATA:
			return new ProfileData(buf);
		case MessageTypes.MSG_PROFILE_TARGET:
			return new ProfileTarget(buf);
		case MessageTypes.MSG_NAMES:
			return new Names(buf);
		case MessageTypes.MSG_STACK_DATA:
			return new StackData(buf);

		case MessageTypes.MSG_ERROR:
			return new ErrorMsg(buf);
		case MessageTypes.MSG_COMMAND_SUCCEED:
			MessageBase msg = new MessageBase();
			msg.messageType = msgid;
			return msg;
		}
		return null;
	}
}

class StartProfile extends MessageBase {
	public final static int METHOD_TABLE = 1;
	public final static int CALL_TREE = 2;

	int profileType;

	public StartProfile() {
		messageType = MessageTypes.MSG_START_PROFILE;
	}

	public void setProfileType(int pt) {
		profileType = pt;
	}

	public int getMessageBodySize() {
		return 4;
	}

	public void putMessageBody(ByteBuffer buf) {
		buf.putInt(profileType);
	}
}

class QueryInfo extends MessageBase {
	public final static int INFO_DATA_SLIDE = 1;

	int infoType;

	public QueryInfo() {
		messageType = MessageTypes.MSG_QUERY_INFO;
	}

	public void setInfoType(int id) {
		infoType = id;
	}

	public int getMessageBodySize() {
		return 4;
	}

	public void putMessageBody(ByteBuffer buf) {
		buf.putInt(infoType);
	}
}

class ReqProfileData extends MessageBase {

	public ReqProfileData() {
		messageType = MessageTypes.MSG_REQ_PROFILE_DATA;
	}
}

class SlideInfo extends MessageBase {

	public HashMap<String, Integer> slide_map;

	public SlideInfo(ByteBuffer buf) {
		slide_map = new HashMap<String, Integer>();
		messageType = MessageTypes.MSG_SLIDE_INFO;
		String retstr = Charset.forName("UTF-8").decode(buf).toString();
		StringTokenizer tok = new StringTokenizer(retstr);
		while (tok.hasMoreTokens()) {
			slide_map.put(tok.nextToken(), Integer.parseInt(tok.nextToken()));
		}
	}

	public void printSlideInfo() {
		for (Map.Entry<String, Integer> entry : slide_map.entrySet()) {
			System.out.println(entry.getKey() + " = "
					+ entry.getValue().toString());
		}
	}

	public int getRecordSize(String prefix) {
		return slide_map.get(prefix + ".record_size");
	}
}

class ProfileTarget extends MessageBase {

	private String name;

	public ProfileTarget(ByteBuffer buf) {
		name = Charset.forName("UTF-8").decode(buf).toString();
	}

	public String getName() {
		return name;
	}
}

class StructArrayMessage extends MessageBase {

	ByteBuffer buffer;
	SlideInfo slide_info;
	int record_size;
	int data_head_size;
	String field_name_prefix = "";

	public StructArrayMessage(ByteBuffer buf) {
		buffer = buf;
		data_head_size = 0;
	}

	public void setSlideInfo(SlideInfo slide_info) {
		this.slide_info = slide_info;
		this.record_size = this.slide_info.getRecordSize(field_name_prefix);
	}

	public int numRecords() {
		return (buffer.capacity() - data_head_size) / record_size;
	}

	public int getSlide(int record_index, String slide_name) {
		int slide = slide_info.slide_map.get(field_name_prefix + "."
				+ slide_name);
		return record_size * record_index + slide + data_head_size;
	}

	public long getLong(int record_index, String slide_name) {
		assert slide_name != "parent_node_id";
		assert slide_name != "call_node_id";
		return buffer.getLong(getSlide(record_index, slide_name));
	}

	public int getInt(int record_index, String slide_name) {
		return buffer.getInt(getSlide(record_index, slide_name));
	}

	public long getLongInPrefix(int offset) {
		return buffer.getLong(offset);
	}
}

class ProfileData extends StructArrayMessage {
	StackData sdata;

	public ProfileData(ByteBuffer buf) {
		super(buf);
		buffer = buf;
		messageType = MessageTypes.MSG_PROFILE_DATA;
		field_name_prefix = "pdata";
		data_head_size = 8;
	}

	public long getTargetThreadID() {
		return getLongInPrefix(0);
	}

	public void print_data() {
		System.out.println("numRecords: " + numRecords());

		System.out.print("mid  class  all_time  count");

		System.out.println();
		for (int i = 0; i < numRecords(); i++) {
			System.out.print(getLong(i, "mid"));
			System.out.print("  ");
			System.out.print(getLong(i, "class"));
			System.out.print("  ");
			System.out.print(getLong(i, "all_time"));
			System.out.print("  ");
			System.out.print(getLong(i, "call_count"));
			System.out.println();
		}
		if (this.sdata != null) {
			System.out.println("[STACK INFO]");
			sdata.print_data();
		}
	}

	public void setStackData(StackData sdata) {
		this.sdata = sdata;
	}

	public StackData getStackData() {
		return sdata;
	}

	public void printStackData() {
		if (sdata == null) {
			System.out.println("NULL");
		} else {
			sdata.print_data();
		}
	}
}

class StackData extends StructArrayMessage {
	public StackData(ByteBuffer buf) {
		super(buf);
		buffer = buf;
		messageType = MessageTypes.MSG_STACK_DATA;
		field_name_prefix = "sdata";
		data_head_size = 8;
	}

	public long getTargetThreadID() {
		return getLongInPrefix(0);
	}

	public void print_data() {
		System.out.println("numRecords: " + numRecords());

		System.out.print("nid  time");

		System.out.println();
		for (int i = 0; i < numRecords(); i++) {
			System.out.print(getInt(i, "node_id"));
			System.out.print("  ");
			System.out.print(getLong(i, "start_time"));
			System.out.print("  ");
		}
	}
}

class QueryNames extends MessageBase {

	int infoType;
	ArrayList<Long> id_list;

	public QueryNames() {
		messageType = MessageTypes.MSG_QUERY_NAMES;
		id_list = new ArrayList<Long>();
	}

	public void addNameID(long nameid) {
		id_list.add(nameid);
	}

	public long getNameID(int idx) {
		return id_list.get(idx);
	}

	public int numMethods() {
		return id_list.size();
	}

	public int getMessageBodySize() {
		return (id_list.size()) * 8 + 16;
	}

	public void putMessageBody(ByteBuffer buf) {
		buf.putInt(MessageTypes.QUERY_NAMES);
		buf.putInt(id_list.size());
		for (Long mid : id_list) {
			buf.putLong(mid);
		}
	}

	public int size() {
		return id_list.size();
	}
}

class Names extends MessageBase {
	int infoType;
	QueryNames query;
	ArrayList<String> name_map;

	public Names(ByteBuffer buf) {
		messageType = MessageTypes.MSG_NAMES;
		name_map = new ArrayList<String>();
		String retstr = Charset.forName("UTF-8").decode(buf).toString();
		int n = 0;
		for (String tok : retstr.split("\n")) {
			name_map.add(tok);
			n++;
		}

	}

	public void setQueryInfo(QueryNames q) {
		query = q;
	}

	public void mergeNamesTo(HashMap<Long, String> dest) {
		for (int i = 0; i < query.numMethods(); i++) {
			assert i < name_map.size();
			dest.put(query.getNameID(i), name_map.get(i));
		}
	}
}

class ErrorMsg extends MessageBase {

	public ErrorMsg(ByteBuffer buf) {
		messageType = MessageTypes.MSG_ERROR;
	}
}
