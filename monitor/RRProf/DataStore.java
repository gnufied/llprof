package RRProf;

import java.util.*;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.TransformerFactoryConfigurationError;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Text;

public class DataStore {

	public interface EventListener {
		void addCallName(CallNameRecordSet call_name);

		void addThread(ThreadStore threadStore);
	}

	public interface RecordEventListener {
		void addRecordChild(AbstractRecord rec);

		void updateRecordData(AbstractRecord rec);

		void updateParentRecordData(AbstractRecord rec);

		void addRecordMember(AbstractRecord rec);
	}

	public abstract class AbstractRecord {
		Set<RecordEventListener> listeners;

		abstract public long getAllValue(int idx);

		abstract public long getAllValueSent(int idx);

		abstract public long getChildrenValue(int idx);

		abstract public long getCallCount();

		abstract public String getRecordName();

		public String getTargetName() {
			return "<error>";
		}

		AbstractRecord() {
			listeners = new HashSet<RecordEventListener>();
		}

		public void addRecordEventListener(RecordEventListener listener) {
			listeners.add(listener);
		}

		public void removeRecordEventListener(RecordEventListener listener) {
			listeners.remove(listener);
		}

		public boolean isRunning() {
			return false;
		}

		public int numRecords() {
			return 1;
		}

		public ChildrenRecord childrenRecord() {
			return new ChildrenRecord(this);
		}

		abstract public int getChildCount();

		abstract public Iterable<AbstractRecord> allChild();

		public long getSelfValue(int idx) {
			if (isRunning())
				return -1;
			else
				return getAllValueSent(idx) - getChildrenValue(idx);
		}
	}

	public class Record extends AbstractRecord {
		public int ID;
		public ThreadStore threadStore;
		public Record parent;
		public long[] allValue;
		public long[] childrenValue;
		public int callCount;
		public long[] runningValue;
		public CallName callName;
		List<AbstractRecord> children;

		public int getID() {
			return ID;
		}

		public long getAllValue(int idx) {
			return allValue[idx] + runningValue[idx];
		}

		public long getAllValueSent(int idx) {
			return allValue[idx];
		}

		public long getChildrenValue(int idx) {
			return childrenValue[idx];
		}

		public long getCallCount() {
			return callCount;
		}

		public Record() {
			children = new ArrayList<AbstractRecord>();
			ID = 0;
			threadStore = null;
			parent = null;
			callName = null;
			allValue = new long[recordMetadata.getNumRecords()];
			childrenValue = new long[recordMetadata.getNumRecords()];
			callCount = 0;
			runningValue = new long[recordMetadata.getNumRecords()];
		}

		public String getPathString() {
			return getPathString(-1);
		}
		public String getPathString(int maxnodes) {
			String result = getTargetName();
			Record current = this.getParent();
			while(current != null) {
				maxnodes--;
				if(maxnodes == 1) {
					while(current.getParent() != null)
						current = current.getParent();
					result = current.getTargetName() + " > ... > " + result;
					break;
				}
				result = current.getTargetName() + " > " + result;
				current = current.getParent();
			}
			return result;
		}
		
		public void exportXML(Document doc, Element parent_elem) {
			Element elem = doc.createElement("node");
			elem.setAttribute("id", Long.toString(ID));
			elem.setAttribute("name", getTargetName());
			if (callName != null) {
				elem.setAttribute("cn", "1");
				elem.setAttribute("nameid", Long.toString(callName.nameid));
			} else {
				elem.setAttribute("cn", "0");
			}
			for(int i = 0; i < recordMetadata.getNumRecords(); i++) {
				elem.setAttribute("all_" + Integer.toString(i), Long.toString(allValue[i]));
				elem.setAttribute("self_" + Integer.toString(i), Long.toString(allValue[i] - childrenValue[i]));
				
			}
			
			elem.setAttribute("count", Long.toString(callCount));

			for (AbstractRecord arec : children) {
				Record rec = (Record) arec;
				if (rec != null)
					rec.exportXML(doc, elem);
			}

			parent_elem.appendChild(elem);
		}

		public ThreadStore getThreadStore() {
			return threadStore;
		}

		public CallName getCallName() {
			return callName;
		}


		public String getTargetName() {
			if (getCallName() != null)
				return getCallName().getName();
			else
				return "(null:j)";
		}

		public boolean equals(Object obj) {
			Record rec = (Record) obj;
			if (rec == null)
				return false;
			return (this.ID == rec.ID);
		}

		public int hashCode() {
			return this.ID;
		}

		public int getChildCount() {
			return children.size();
		}

		public Record getChild(int idx) {
			return (Record) children.get(idx);
		}

		public Iterable<AbstractRecord> allChild() {
			return children;
		}

		public int getIndexOfChild(Record child) {
			return children.indexOf(child);
		}

		public void addChild(Record rec) {
			children.add(rec);
			for (RecordEventListener l : listeners) {
				l.addRecordChild(rec);
			}
		}

		public void callUpdateDataEvent() {
			for (RecordEventListener l : listeners) {
				l.updateRecordData(this);
			}
			for (AbstractRecord rec : this.children) {
				((Record) rec).callUpdateParentDataEvent();
			}
		}

		public void callUpdateParentDataEvent() {
			for (RecordEventListener l : listeners) {
				l.updateParentRecordData(this);
			}
		}

		public String toString() {
			return "R:" + getTargetName();
		}

		public String getLabelString(String qlabel, int division) {
			/// @todo index 0 
			if (callName == null)
				return "Null Record";
			else {
				int p = getPercentage(0);
				if (p == -1)
					return String.format("%s %d" + qlabel, getCallName()
							.getName(), getAllValue(0) / division);
				else
					return String.format("%s %d" + qlabel + "(%d%%)",
							getCallName().getName(), getAllValue(0) / division,
							getPercentage(0));
			}
		}

		public String getRecordName() {
			return getTargetName();
		}

		public Record getParent() {
			return parent;
		}

		public int childIndex(Record child) {
			return children.indexOf(child);
		}

		public int getPercentage(int idx) {
			if (parent == null)
				return 100;
			if (parent.getAllValue(idx) == 0)
				return -1;
			return (int) (100 * getAllValue(idx) / parent.getAllValue(idx));
		}

		public void addRunningValue(int index, long n) {
			runningValue[index] += n;
		}

		public void subRunningValue(int index, long n) {
			runningValue[index] -= n;
		}

		public long getRunningValue(int index) {
			return runningValue[index];
		}

		public boolean isRunning() {
			/// \todo フラグを作る
			return runningValue[0] != 0;
		}

		public int numRecords() {
			return 1;
		}

	}

	public class RecordSet extends AbstractRecord implements
			RecordEventListener, Iterable<AbstractRecord> {
		Set<AbstractRecord> record_set;
		String name = "";

		public RecordSet() {
			record_set = new HashSet<AbstractRecord>();
		}

		public Iterator<AbstractRecord> iterator() {
			return record_set.iterator();
		}

		public boolean isRunning() {
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					if (rec.isRunning())
						return true;
				}
			}
			return false;
		}

		@Override
		public long getAllValue(int i) {
			long result = 0;
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					result += rec.getAllValue(i);
				}
			}
			return result;
		}

		@Override
		public long getAllValueSent(int i) {
			long result = 0;
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					result += rec.getAllValueSent(i);
				}
			}
			return result;
		}

		@Override
		public long getCallCount() {
			long result = 0;
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					result += rec.getCallCount();
				}
			}
			return result;
		}

		@Override
		public long getChildrenValue(int idx) {
			long result = 0;
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					result += rec.getChildrenValue(idx);
				}
			}
			return result;
		}

		@Override
		public String getRecordName() {
			return this.name;
		}

		public void setRecordName(String name) {
			this.name = name;
		}

		public void add(AbstractRecord rec) {
			boolean flag;
			synchronized (record_set) {
				flag = record_set.add(rec);
			}
			if (flag) {
				rec.addRecordEventListener(this);
				callAddRecordMember(rec);
				for (AbstractRecord child : rec.allChild()) {
					addRecordChild(child);
				}
			}
		}

		public void remove(AbstractRecord rec) {
			synchronized (record_set) {
				record_set.remove(rec);
				rec.removeRecordEventListener(this);
			}
		}

		public void callAddRecordMember(AbstractRecord newmember) {
			for (RecordEventListener l : listeners) {
				l.addRecordMember(newmember);
			}
		}

		@Override
		public void addRecordChild(AbstractRecord rec) {
			for (RecordEventListener l : listeners) {
				l.addRecordChild(rec);
			}
		}

		@Override
		public void updateParentRecordData(AbstractRecord rec) {
		}

		@Override
		public void updateRecordData(AbstractRecord rec) {
			for (RecordEventListener l : listeners) {
				l.updateRecordData(this);
			}
		}

		public int numRecords() {
			synchronized (record_set) {
				return record_set.size();
			}
		}

		@Override
		public void addRecordMember(AbstractRecord rec) {
		}

		class ChildIterator implements Iterable<AbstractRecord>,
				Iterator<AbstractRecord> {

			Iterator<AbstractRecord> record_set_iter;
			Iterator<AbstractRecord> parent_iter;
			AbstractRecord nextStock;

			@Override
			public Iterator<AbstractRecord> iterator() {
				record_set_iter = record_set.iterator();
				if (record_set_iter.hasNext())
					parent_iter = record_set_iter.next().allChild().iterator();
				else
					parent_iter = null;
				getStock();
				return this;
			}

			@Override
			public boolean hasNext() {
				return nextStock != null;
			}

			@Override
			public AbstractRecord next() {
				AbstractRecord result = nextStock;
				getStock();
				return result;
			}

			private void getStock() {
				while (parent_iter == null || !parent_iter.hasNext()) {
					if (!record_set_iter.hasNext()) {
						nextStock = null;
						return;
					}
					parent_iter = record_set_iter.next().allChild().iterator();
				}
				nextStock = parent_iter.next();
			}

			@Override
			public void remove() {
			}

		}

		public Iterable<AbstractRecord> allChild() {
			return new ChildIterator();
		}

		@Override
		public int getChildCount() {
			int result = 0;
			synchronized (record_set) {
				for (AbstractRecord rec : record_set) {
					result += rec.getChildCount();
				}
			}
			return result;
		}

	}

	public class CallNameRecordSet extends RecordSet {
		CallName callName;

		public CallNameRecordSet(CallName callName) {
			super();
			this.callName = callName;
		}

		public CallName getCallName() {
			return callName;
		}

		@Override
		public boolean equals(Object obj) {
			if (!(obj instanceof CallNameRecordSet))
				return false;
			CallNameRecordSet recset = (CallNameRecordSet) obj;
			return getCallName().equals(recset.getCallName());
		}

		@Override
		public int hashCode() {
			return getCallName().hashCode();
		}

		public String getTargetName() {
			return getCallName().getName();
		}

	}

	public class ChildrenRecord extends RecordSet {
		String name = "";
		AbstractRecord parent;

		public class ParentListener implements RecordEventListener {
			@Override
			public void addRecordChild(AbstractRecord rec) {
				add(rec);
			}

			@Override
			public void addRecordMember(AbstractRecord rec) {
			}

			@Override
			public void updateParentRecordData(AbstractRecord rec) {
			}

			@Override
			public void updateRecordData(AbstractRecord rec) {
			}
		}

		public ChildrenRecord(AbstractRecord p) {
			parent = p;
			for (AbstractRecord child : p.allChild()) {
				add(child);
			}
			parent.addRecordEventListener(new ParentListener());
		}

		@Override
		public String getRecordName() {
			return "Children of " + parent.getRecordName();
		}

		@Override
		public void addRecordChild(AbstractRecord rec) {
			for (RecordEventListener l : listeners) {
				l.addRecordChild(rec);
			}
		}

	}

	public class CallName {
		public CallName(long nameid) {
			this.nameid = nameid;
		}

		public long nameid;

		public boolean equals(Object obj) {
			CallName cn = (CallName) obj;
			if (cn == null)
				return false;
			return (this.nameid == cn.nameid);
		}

		public int hashCode() {
			return (int) (this.nameid);
		}

		public String toString() {
			return Long.toString(this.nameid);
		}

		public String getName() {
			return name_id_map.get(nameid);
		}
	}

	public class ThreadStore {
		List<Record> callNodes;
		List<RunningRecordInfo> runningRecords;
		long threadID;

		ThreadStore(long thread_id) {
			callNodes = new ArrayList<Record>();
			runningRecords = new ArrayList<RunningRecordInfo>();
			threadID = thread_id;
		}

		public Record getNodeRecord(long nodeID_long) {
			int nodeID = (int) nodeID_long;
			// ほとんどの場合先頭から追加されるからwhileループで追加してok
			// 必要であればensureCapacityなど
			while (callNodes.size() <= nodeID)
				callNodes.add(null);
			Record record = callNodes.get(nodeID);
			if (record == null) {
				record = new Record();
				record.ID = nodeID;
				record.threadStore = this;
				callNodes.set(nodeID, record);
			}
			return record;

		}

		public int numNodes() {
			return callNodes.size();
		}

		List<RunningRecordInfo> getRunningRecords() {
			return runningRecords;
		}

		public Record getRootRecord() {
			return getNodeRecord(1);
		}

		public long getThreadID() {
			return threadID;
		}

		public void exportXML(Document doc, Element elem) {
			if (getRootRecord() != null) {
				getRootRecord().exportXML(doc, elem);
			} else {
				System.out.println("Export Error: No root");
			}
		}
	}

	Set<EventListener> listeners;

	class RunningRecordInfo {
		public Record target;
		public long[] startValue;
		public long[] runningValue;
		
		RunningRecordInfo() {
			startValue = new long[recordMetadata.getNumRecords()];
			runningValue = new long[recordMetadata.getNumRecords()];
		}
	}

	Map<Long, ThreadStore> threadStores;

	Map<CallName, CallNameRecordSet> name_to_node;
	Map<Long, String> name_id_map;

	DataStore() {
		name_to_node = new HashMap<CallName, CallNameRecordSet>();
		listeners = new HashSet<EventListener>();
		threadStores = new HashMap<Long, ThreadStore>();
	}
	
	RecordMetadata recordMetadata;
	
	public void setRecordMetadata(RecordMetadata rm) {
		recordMetadata = rm;
	}

	public ThreadStore addThreadStore(long thread_id) {
		assert !threadStores.containsKey(thread_id);
		ThreadStore ts = new ThreadStore(thread_id);
		threadStores.put(thread_id, ts);

		callAddThreadEvent(ts);
		return ts;
	}

	public ThreadStore getThreadStore(long thread_id, boolean add_new) {
		if (add_new && !threadStores.containsKey(thread_id)) {
			return addThreadStore(thread_id);
		}
		return threadStores.get(thread_id);
	}

	public ThreadStore getThreadStore(long thread_id) {
		return getThreadStore(thread_id, false);
	}

	public long numNodesOfThread(long thread_id) {
		return getThreadStore(thread_id).numNodes();
	}

	public long numAllNodes() {
		long result = 0;
		for (Map.Entry<Long, ThreadStore> entry : threadStores.entrySet()) {
			result += entry.getValue().numNodes();
		}
		return result;
	}

	public void addEventListener(EventListener l) {
		listeners.add(l);
	}

	public void removeEventListener(EventListener l) {
		listeners.remove(l);
	}

	public int numCallNames() {
		return name_to_node.size();
	}

	public Record getRootRecord(long thread_id) {
		return getThreadStore(thread_id).getRootRecord();
	}

	public void setNameMap(Map<Long, String> map) {
		name_id_map = map;
	}

	public Record getNodeRecord(long thread_id, int cnid) {
		return getThreadStore(thread_id).getNodeRecord(cnid);
	}

	public void recordData(ProfileData data) {
		long target_thread = data.getTargetThreadID();
		ThreadStore thread_store = getThreadStore(target_thread, true);
		List<RunningRecordInfo> running_records = thread_store.runningRecords;

		// 実行中補正の記録
		StackData sdata = data.getStackData();
		for(int recidx = 0; recidx < recordMetadata.getNumRecords(); recidx++) {
			long nowTime = sdata.getLong(0, "start_value", recidx);
	
			while (sdata.numRecords() - 1 < running_records.size()) {
				RunningRecordInfo runrec = running_records.get(running_records
						.size() - 1);
				running_records.remove(running_records.size() - 1);
				assert (runrec != null);
				assert (runrec.target != null);
				if (runrec.target != null) {
					runrec.target.subRunningValue(recidx, runrec.runningValue[recidx]);
					runrec.target.callUpdateDataEvent();
				}
			}
			while (sdata.numRecords() - 1 > running_records.size()) {
				running_records.add(new RunningRecordInfo());
			}
			assert (running_records.size() == sdata.numRecords() - 1);
			for (int i = 1; i < sdata.numRecords(); i++) {
				RunningRecordInfo rrec = running_records.get(i - 1);
				int node_id = sdata.getInt(i, "node_id");
				if (node_id != 0) {
					if (rrec.target != null) {
						rrec.target.subRunningValue(recidx, rrec.runningValue[recidx]);
						rrec.target.callUpdateDataEvent();
					}
					rrec.target = thread_store.getNodeRecord(node_id);
					rrec.startValue[recidx] = sdata.getLong(i, "start_value", recidx);
					rrec.runningValue[recidx] = 0;
				}
				if (rrec.target != null) {
					long old_rt = rrec.runningValue[recidx];
					rrec.runningValue[recidx] = nowTime - rrec.startValue[recidx];
					rrec.target.addRunningValue(recidx, rrec.runningValue[recidx] - old_rt);
					rrec.target.callUpdateDataEvent();
				}
			}
		}

		// 実行データの記録
		for (int i = 0; i < data.numRecords(); i++) {
			CallName cnid = new CallName(data.getLong(i, "nameid"));
			CallNameRecordSet rec_set;
			if (!name_to_node.containsKey(cnid)) {
				rec_set = new CallNameRecordSet(cnid);
				name_to_node.put(cnid, rec_set);
				callAddCallNameEvent(rec_set);
			} else {
				rec_set = name_to_node.get(cnid);
			}
			int call_node_id = (int) data.getInt(i, "call_node_id");
			Record record = thread_store.getNodeRecord(call_node_id);
			rec_set.add(record);
			if (record.parent == null) {
				record.parent = thread_store.getNodeRecord((int) data.getInt(i,
						"parent_node_id"));
				record.callName = cnid;
				record.parent.addChild(record);
			}
			for(int j = 0; j < recordMetadata.getNumRecords(); j++) {
				record.allValue[j] += data.getLong(i, "value", j);
				if (record.parent != null)
					record.parent.childrenValue[j] += data.getLong(i, "value", j);
			}
			record.callCount += data.getLong(i, "call_count");
			record.callUpdateDataEvent();
		}
	}

	public void callAddCallNameEvent(CallNameRecordSet rset) {
		for (EventListener l : listeners) {
			l.addCallName(rset);
		}
	}

	public void callAddThreadEvent(ThreadStore threadStore) {
		for (EventListener l : listeners) {
			l.addThread(threadStore);
		}
	}

	public CallNameRecordSet getRecordOfCallName(CallName cn) {
		return name_to_node.get(cn);
	}

	public void exportXML(String filename) {
		DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
		DocumentBuilder db;
		Document document = null;
		try {
			db = dbf.newDocumentBuilder();
			document = db.newDocument();
		} catch (ParserConfigurationException e) {
			e.printStackTrace();
		}

		Element root_elem = document.createElement("profile");
		document.appendChild(root_elem);
		for (Map.Entry<Long, ThreadStore> e : threadStores.entrySet()) {
			Element thread_elem = document.createElement("thread");
			thread_elem.setAttribute("ID", e.getKey().toString());
			e.getValue().exportXML(document, thread_elem);
			root_elem.appendChild(thread_elem);
		}

		File file = new File(filename);
		PrintWriter pwriter;
		try {
			pwriter = new PrintWriter(new BufferedWriter(new FileWriter(file)));
			TransformerFactory.newInstance().newTransformer().transform(
					new DOMSource(document), new StreamResult(pwriter));
		} catch (TransformerConfigurationException e1) {
			e1.printStackTrace();
		} catch (TransformerException e1) {
			e1.printStackTrace();
		} catch (TransformerFactoryConfigurationError e1) {
			e1.printStackTrace();
		} catch (IOException e1) {
			e1.printStackTrace();
		}
	}

	public RecordMetadata getRecordMetadata() {
		return recordMetadata;
	}
}
