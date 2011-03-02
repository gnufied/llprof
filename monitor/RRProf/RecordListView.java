package RRProf;

import java.awt.Point;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JTable;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.event.TableModelEvent;
import javax.swing.event.TableModelListener;
import javax.swing.event.TreeModelListener;
import javax.swing.table.TableModel;

import RRProf.CallTreeBrowserView.CallTreeBrowserModel;
import RRProf.CallTreeBrowserView.CallTreeRenderer;
import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallName;
import RRProf.DataStore.CallNameRecordSet;

public class RecordListView extends JTable implements ListSelectionListener{

	
	static final int COL_RUNNING_ICON = 1;
	static final int COL_CLASS = 2;
	static final int COL_METHOD = 3;
	static final int COL_ALL_TIME = 4;
	static final int COL_SELF_TIME = 5;
	static final int COL_NUM_NODES = 6;
	static final int COL_NUM_CALLS = 7;
	
	ArrayList<Integer> columns;
	
	class MethodTableModel implements TableModel{
		Set<TableModelListener> listeners;
		Icon runningIcon, normalIcon;
		public MethodTableModel()
		{
			listeners = new HashSet<TableModelListener>();
			runningIcon = new ImageIcon("icons/running.png");
			normalIcon = new ImageIcon("icons/normal.png");
		}
		
		public void addTableModelListener(TableModelListener l) {
			listeners.add(l);
		}

		public Class<?> getColumnClass(int columnIndex) {
			switch(columns.get(columnIndex)) {
			case COL_RUNNING_ICON:
				return Icon.class;
			case COL_CLASS:
				return String.class;
			case COL_METHOD:
				return String.class;
			case COL_ALL_TIME:
				return Long.class;
			case COL_SELF_TIME:
				return Long.class;
			case COL_NUM_NODES:
				return Integer.class;
			case COL_NUM_CALLS:
				return Long.class;
			}
			return Object.class;
		}

		@Override
		public int getColumnCount() {
			return columns.size();
		}

		@Override
		public String getColumnName(int columnIndex) {
			switch(columns.get(columnIndex)) {
			case COL_RUNNING_ICON:	return "Run";
			case COL_METHOD:			return "Method";
			case COL_CLASS:			return "Class";
			case COL_ALL_TIME:		return "AllTime";
			case COL_SELF_TIME:		return "SelfTime";
			case COL_NUM_NODES:	return "numRecords";
			case COL_NUM_CALLS:	return "numCalls";
			}
			return "!!";
		}

		@Override
		public int getRowCount() {
			return numRows;
		}

		@Override
		synchronized public Object getValueAt(int rowIndex, int columnIndex) {
			AbstractRecord rec = (AbstractRecord)call_name_idx.get(rowIndex);
			if(rec == null)
				return "";
			switch(columns.get(columnIndex)) {
			case COL_RUNNING_ICON:
				return rec.isRunning() ? runningIcon : normalIcon;
			case COL_CLASS:
				return rec.getTargetClass();
			case COL_METHOD:
				return rec.getTargetMethodName();
			case COL_ALL_TIME:
				return rec.getAllTime();
			case COL_SELF_TIME:
				return rec.getSelfTime();
			case COL_NUM_NODES:
				return rec.numRecords();
			case COL_NUM_CALLS:
				return rec.getCallCount();
			}
			return "!!";
		}

		@Override
		public boolean isCellEditable(int rowIndex, int columnIndex) {
			return false;
		}

		@Override
		public void removeTableModelListener(TableModelListener l) {
			listeners.remove(l);
		}

		@Override
		public void setValueAt(Object aValue, int rowIndex, int columnIndex) {
			
		}
		public void callInsertEvent(AbstractRecord rec)
		{
			Integer idx = call_name_idx_rev.get(rec);
			TableModelEvent e;
			assert(idx != null);
			e = new TableModelEvent(this, idx, idx, TableModelEvent.ALL_COLUMNS, TableModelEvent.INSERT);
			
			callChangedEvent(e);
		}
		
		public void callChangedEvent(AbstractRecord rec)
		{
			Integer idx = call_name_idx_rev.get(rec);
			TableModelEvent e;
			if(idx == null)
				e = new TableModelEvent(this);
			else
				e = new TableModelEvent(this, idx);
			
			callChangedEvent(e);
		}

		synchronized public void callChangedEvent(TableModelEvent e)
		{
			for(TableModelListener l: listeners)
			{
				l.tableChanged(e);
			}
		}
		public void updateColumn() {
			callChangedEvent(new TableModelEvent(this, TableModelEvent.HEADER_ROW));
		}

	}
	
	int numRows;
	
	Map<Integer, AbstractRecord> call_name_idx;
	Map<AbstractRecord, Integer> call_name_idx_rev;
	MethodTableModel model;
	public RecordListView()
	{
		super();
		columns = new ArrayList<Integer>();
		call_name_idx = new HashMap<Integer, AbstractRecord>();
		call_name_idx_rev = new HashMap<AbstractRecord, Integer>();
		model = new MethodTableModel();
		
		setModel(model);
		setAutoCreateRowSorter(true);
		 numRows = 0;
		 
	}
	
	public void updateColumn() {
		model.updateColumn();
	}
	
	public int getRowCount() {
		return numRows;
	}
	public void addRecord(AbstractRecord rec){
		synchronized(model)
		{
			assert(!call_name_idx_rev.containsKey(rec));
			call_name_idx.put(call_name_idx.size(), rec);
			call_name_idx_rev.put(rec, call_name_idx_rev.size());
			assert(call_name_idx.size() == call_name_idx_rev.size());
			numRows++;
			model.callInsertEvent(rec);
		}
	}

	public void recordChanged(AbstractRecord rec) {
		synchronized(model)
		{
			model.callChangedEvent(rec);
		}
	}

	
	public void valueChanged(ListSelectionEvent e) {
	    // if(e.getValueIsAdjusting()) return;

	    // DataStore.RecordSet recset = (DataStore.RecordSet)call_name_idx.get(e.getFirstIndex());
	    
	}

	public AbstractRecord getRecordAt(Point pt) {
		int row = convertRowIndexToModel(rowAtPoint(pt));
		return call_name_idx.get(row);
		
	}


}
