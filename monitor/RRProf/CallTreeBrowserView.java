package RRProf;

import java.awt.Component;
import java.awt.Rectangle;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.Set;

import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JTree;
import javax.swing.event.TreeModelEvent;
import javax.swing.event.TreeModelListener;
import javax.swing.tree.DefaultTreeCellRenderer;
import javax.swing.tree.TreeModel;
import javax.swing.tree.TreePath;

import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallNameRecordSet;
import RRProf.DataStore.Record;
import RRProf.DataStore.ThreadStore;

public class CallTreeBrowserView extends JTree implements KeyListener {
	
	abstract class CallTreeBrowserNode {
		abstract public Object getChild(int index);
		abstract int getChildCount(); 
		abstract int getIndexOfChild(Object child);
		abstract boolean isLeaf();
		abstract String getLabelString();
	}
	
	public class ThreadNode extends CallTreeBrowserNode {
		long threadID;
		ThreadStore threadDataStore;
		int index;
		
		ThreadStore getThreadStore() {
			return threadDataStore;
		}
		public int getIndex() { return index; }
		ThreadNode(ThreadStore thread_data_store, int idx) {
			index = idx;
			threadDataStore = thread_data_store;
			threadID = threadDataStore.getThreadID();
		}
		
		public Record getRootRecord() {
			return threadDataStore.getRootRecord();
		}
		
		@Override
		public Object getChild(int index) {
			return threadDataStore.getRootRecord().getChild(index);
		}
		@Override
		int getChildCount() {
			return threadDataStore.getRootRecord().getChildCount();
		}
		@Override
		int getIndexOfChild(Object child) {
			return threadDataStore.getRootRecord().getIndexOfChild((DataStore.Record)child);
		}
		@Override
		String getLabelString() {
			return "Thread" + Long.toString(threadID);
		}
		@Override
		boolean isLeaf() {
			return getChildCount() == 0;
		}
	}

	public class RootNode extends CallTreeBrowserNode {
		Map<Long, ThreadNode> threadID_set;
		Map<Integer, ThreadNode> threadID_index;
		
		int index_num;
		RootNode() {
			threadID_set = new HashMap<Long, ThreadNode>();
			threadID_index = new HashMap<Integer, ThreadNode>();
			index_num = 0;
		}
		
		public ThreadNode addThread(ThreadStore ts) {
			ThreadNode tn = new ThreadNode(ts, index_num);
			threadID_set.put(ts.getThreadID(), tn);
			threadID_index.put(index_num, tn);
			index_num++;
			return tn;
		}
		
		public ThreadNode getThreadNode(long threadID) {
			return threadID_set.get(threadID);
		}

		@Override
		public Object getChild(int index) {
			return threadID_index.get(index);
		}

		@Override
		int getChildCount() {
			return threadID_index.size();
		}

		@Override
		int getIndexOfChild(Object child) {
			ThreadNode node = (ThreadNode)child;
			return node.getIndex();
		}

		@Override
		String getLabelString() {
			return "Root";
		}

		@Override
		boolean isLeaf() {
			return threadID_set.isEmpty();
		}
	}
	
	RootNode rootNode;

    public class CallTreeBrowserModel implements TreeModel, DataStore.RecordEventListener, DataStore.EventListener
    {
        Set<TreeModelListener> listeners;
        DataStore dataStore;
        public CallTreeBrowserModel(DataStore dataStore)
        {
            this.dataStore = dataStore;
            listeners = new HashSet<TreeModelListener>();
            this.dataStore.addEventListener(this);
        }
        public void addTreeModelListener(TreeModelListener l)
        {
            listeners.add(l);
        }
        public void removeTreeModelListener(TreeModelListener l)
        {
            listeners.remove(l);
        }
        public Object getChild(Object parent, int index)
        {
        	if(parent instanceof CallTreeBrowserNode) {
        		CallTreeBrowserNode node = (CallTreeBrowserNode)parent;
        		return node.getChild(index);
        	} else {
				DataStore.Record record = (DataStore.Record)parent;
				return record.getChild(index);
        	}
        }
        public int getChildCount(Object parent)
        {
        	if(parent instanceof CallTreeBrowserNode) {
        		CallTreeBrowserNode node = (CallTreeBrowserNode)parent;
        		return node.getChildCount();        		
        	} else {
            DataStore.Record record = (DataStore.Record)parent;
            return record.getChildCount();
        	}
        }
        public int getIndexOfChild(Object parent, Object child)
        {
        	if(parent instanceof CallTreeBrowserNode) {
        		CallTreeBrowserNode node = (CallTreeBrowserNode)parent;
        		return node.getIndexOfChild(child);        		
        	} else {
            DataStore.Record record = (DataStore.Record)parent;
            DataStore.Record child_rec = (DataStore.Record)child;
            return record.getIndexOfChild(child_rec);
        	}
        }
        public Object getRoot()
        {
            return rootNode;
        }
        public boolean isLeaf(Object node)
        {
        	if(node instanceof CallTreeBrowserNode) {
        		CallTreeBrowserNode cnode = (CallTreeBrowserNode)node;
        		return cnode.isLeaf();
        	} else {
            DataStore.Record record = (DataStore.Record)node;
            return record.getChildCount() == 0;
        	}
        }
        public void valueForPathChanged(TreePath path, Object newValue)
        {
        }
        
       public void addRecordChild(DataStore.AbstractRecord arec) {
    	   DataStore.Record rec = (DataStore.Record)arec;
    	   rec.addRecordEventListener(this);
    	   if(listeners.isEmpty())
   				return;
    	   LinkedList inserted_list;
    	   inserted_list = new LinkedList();
    	   for(DataStore.Record i = rec; i != null; i = i.getParent()){
   				inserted_list.addFirst(i);
   			}
    	   int idxes[] = {rec.getParent().childIndex(rec)};
    	   Object children[] = {rec};
    	   TreeModelEvent event = new TreeModelEvent(this, inserted_list.toArray(), idxes, children);
    	   for(TreeModelListener l: listeners) {
    		   l.treeNodesInserted(event);
    	   }
    	   /*
		   System.out.print(":: Add [");
		   System.out.print(inserted_list.size());
		   System.out.print("] ");		   
		   for(Object ro: inserted_list)
    	   {
    		   DataStore.Record r = (DataStore.Record)ro;
    		   System.out.print("->");
    		   if(r.getCallName() != null)
    		   {
	    		   System.out.print(r.getTargetName());
	    		   System.out.print("(");
	    		   System.out.print(r.getCallName().nameid);
	    		   System.out.print(")");
    		   }
    		   else{
	    		   System.out.print("[!]");
    		   }
    	   }
    	   System.out.println();
    	   */
       }

	   public void reload()
	   {
          	Object path[] = {rootNode};
           	TreeModelEvent event = new TreeModelEvent(this, path);
	      	for(TreeModelListener l: listeners) {
	       		l.treeStructureChanged(event);
	       		}
	   }
       public void updateRecordData(DataStore.AbstractRecord arec) {
     	   DataStore.Record rec = (DataStore.Record)arec;
        	updateTreeNode(rec);
        }
        
        
        public Object[] objectToPath(Object obj) {
         	LinkedList path = new LinkedList();
        	if(obj instanceof DataStore.Record) {
        		
	       	for(DataStore.Record i = (DataStore.Record)obj; i != null; i = i.getParent()){
	       		path.addFirst(i);
	       		}
	       	DataStore.Record root_record = (DataStore.Record)path.get(0);
	       	obj = rootNode.getThreadNode(root_record.getThreadStore().getThreadID());
	       	path.remove(0);
        	}
        	if(obj instanceof ThreadNode) {
        		path.addFirst(obj);
        		obj = rootNode;
        	}
        	if(obj instanceof RootNode) {
        		path.addFirst(obj);
        	}
        	return path.toArray();
        }
        public void updateTreeNode(Object obj) {
           	if(listeners.isEmpty())
       			return;
       	LinkedList path = new LinkedList();
       	
       	TreeModelEvent event = new TreeModelEvent(this, objectToPath(obj));
       	for(TreeModelListener l: listeners) {
       		l.treeNodesChanged(event);
       		}
    	}
		public void updateParentRecordData(DataStore.AbstractRecord arec) {
	    	   DataStore.Record rec = (DataStore.Record)arec;
        	updateTreeNode(rec);
		}
		@Override
		public void addRecordMember(AbstractRecord rec) {
		}
		@Override
		public void addThread(ThreadStore threadStore) {
			ThreadNode thread_node = rootNode.addThread(threadStore);
			threadStore.getRootRecord().addRecordEventListener(this);

       	if(listeners.isEmpty())
       			return;
       	LinkedList inserted_list;
       	Object path[] = {rootNode, thread_node};
       	int idxes[] = {thread_node.getIndex()};
       	Object children[] = {thread_node};

       	TreeModelEvent event = new TreeModelEvent(this, path, idxes, children);
       	for(TreeModelListener l: listeners) {
       		l.treeNodesInserted(event);
       		}
		}
		@Override
		public void addCallName(CallNameRecordSet callName) {
		}
        

    }
   
    
    class CallTreeRenderer extends DefaultTreeCellRenderer {
        Icon runningIcon, normalIcon;

        public Icon loadIcon(String fn)
        {
        	return new ImageIcon(fn);
        }
        
        public CallTreeRenderer() {
        	normalIcon = loadIcon("icons/normal.png");
        	runningIcon = loadIcon("icons/running.png");

        }

        public Component getTreeCellRendererComponent(
                            JTree tree,
                            Object value,
                            boolean sel,
                            boolean expanded,
                            boolean leaf,
                            int row,
                            boolean hasFocus) {

            super.getTreeCellRendererComponent(
                            tree, value, sel,
                            expanded, leaf, row,
                            hasFocus);

            if(value instanceof CallTreeBrowserNode) {
            	CallTreeBrowserNode node = (CallTreeBrowserNode)value;
	            this.setText(node.getLabelString());
            		setIcon(runningIcon);
	            setToolTipText("Thread");
            } else {
	            DataStore.Record rec = (DataStore.Record)value;
	            this.setText(rec.getLabelString(mm_qlabel, mm_division));
	            if(rec.isRunning())
	            	setIcon(runningIcon);
	            else
	           	setIcon(normalIcon);
	            setToolTipText("call node");
            }
            return this;
        }
    }

	CallTreeBrowserModel model;
	CallTreeRenderer renderer;
	Monitor mon;
	String mm_qlabel;
	int mm_division;
	public void setMonitor(Monitor m)
	{
		mon = m;
		model = new CallTreeBrowserModel(mon.getDataStore());
		renderer = new CallTreeRenderer();
		setModel(model);
		setCellRenderer(renderer);
	}
	
	public CallTreeBrowserView()
	{
		super();
		mm_qlabel = "";
		mm_division = 1;
		rootNode = new RootNode();
		setModel(null);
		setCellRenderer(null);
		setScrollsOnExpand(true);
		this.addKeyListener(this);
	}
	
	public void setMeasureMode(String qlabel, int division)
	{
		if(qlabel == null)
		{
			mm_qlabel = "";
		}
		else
		{
			mm_qlabel = qlabel;
		}
		mm_division = division;
	}

	@Override
	public void keyPressed(KeyEvent e) {
		if(e.getKeyCode() == KeyEvent.VK_F5)
		{
			System.out.println("Refresh!!");
			TreePath selected = getSelectionPath();
			((CallTreeBrowserModel)this.getModel()).reload();
			
			scrollPathToVisible(selected);
			setSelectionPath(selected);
			
			
		}
	}

	@Override
	public void keyReleased(KeyEvent e) {
	}

	@Override
	public void keyTyped(KeyEvent e) {
	}
	
	static final int OTM_ACTIVE = 1;
	static final int OTM_BOTTLENECK = 2;

	public void openTree(Record record){
		if(record == null)
			return;
		LinkedList path = new LinkedList();
		while(record.getParent() != null) {
			path.addFirst(record);
			record = record.getParent();
		}
		RootNode tree_root = ((RootNode)model.getRoot());
		int nThreads = tree_root.getChildCount();
		for(int i = 0; i < nThreads; i++)
		{
			ThreadNode th_node = (ThreadNode)tree_root.getChild(i);
			if(record.getThreadStore() == th_node.getThreadStore())
			{
				path.addFirst(th_node);
				path.addFirst(tree_root);
				openTree(new TreePath(path.toArray()));
				return;
			}
		}
	}

	public void openTree(TreePath path){
		System.out.print("Open: ");
		for(int i = 0; i < path.getPath().length; i++){
			System.out.print(path.getPath()[i].toString());
			System.out.print(" > ");
			
		}
		System.out.println("]");
		
		scrollPathToVisible(path);
		setSelectionPath(path);
	}

	public void openTree(int open_mode){
		((CallTreeBrowserModel)this.getModel()).reload();
		RootNode tree_root = ((RootNode)model.getRoot());
		int nThreads = tree_root.getChildCount();
		for(int i = 0; i < nThreads; i++)
		{
			ThreadNode th_node = (ThreadNode)tree_root.getChild(i);
			AbstractRecord node = th_node.getRootRecord();

			ArrayList path = new ArrayList();
			path.add(tree_root);
			path.add(th_node);
			while(true)
			{
				AbstractRecord next = null;
				if(open_mode == OTM_ACTIVE)
				{
					for(AbstractRecord rec: node.allChild())
					{
						if(rec.isRunning())
						{
							next = rec;
							break;
						}
					}
				}
				else if(open_mode == OTM_BOTTLENECK)
				{
					long current_heavy_val = 0;
					for(AbstractRecord rec: node.allChild())
					{
						/// @todo index zero
						if(rec.getAllValue(0) > current_heavy_val)
						{
							current_heavy_val = rec.getAllValue(0);
							next = rec;
						}
					}
				}
				if(next == null)
					break;
				path.add(next);
				node = next;
			}
			openTree(new TreePath(path.toArray()));
			return;
		}
	}
	
}
