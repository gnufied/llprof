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

public class RecordListView extends JTable implements ListSelectionListener {

	static final int COL_RUNNING_ICON = 1;
	static final int COL_NODENAME = 2;
	static final int COL_NUM_NODES = 5;
	static final int COL_NUM_CALLS = 6;
	static final int COL_RECORDS = 100;

	static final int COL_RECORD_ALL(int idx)
	{
		return COL_RECORDS + idx*2;
	}
	static final int COL_RECORD_SELF(int idx)
	{
		return COL_RECORDS + idx*2 + 1;
	}

	ArrayList<Integer> columns;

	class MethodTableModel implements TableModel {
		Set<TableModelListener> listeners;
		Icon runningIcon, normalIcon;

		public MethodTableModel() {
			listeners = new HashSet<TableModelListener>();
			runningIcon = new ImageIcon("icons/running.png");
			normalIcon = new ImageIcon("icons/normal.png");
		}

		public void addTableModelListener(TableModelListener l) {
			listeners.add(l);
		}

		public Class<?> getColumnClass(int columnIndex) {
			switch (columns.get(columnIndex)) {
			case COL_RUNNING_ICON:
				return Icon.class;
			case COL_NODENAME:
				return String.class;
			case COL_NUM_NODES:
				return Integer.class;
			case COL_NUM_CALLS:
				return Long.class;
			}
			if(columns.get(columnIndex) >= COL_RECORDS) {
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
			int cidx = columns.get(columnIndex);
			switch (cidx) {
			case COL_RUNNING_ICON:
				return "Run";
			case COL_NODENAME:
				return "Name";
			case COL_NUM_NODES:
				return "numRecords";
			case COL_NUM_CALLS:
				return "numCalls";
			}
			if(cidx >= COL_RECORDS) {
				if(dataStore == null || dataStore.getRecordMetadata() == null)
					return "Unknown";
				int ridx = (cidx - COL_RECORDS) / 2;
				boolean is_all = (cidx - COL_RECORDS) % 2 == 0;
				String rname = dataStore.getRecordMetadata().getRecordName(ridx);
				if(is_all)
					rname = "All " + rname;
				else
					rname = "Self " + rname;
				return rname;
			}
			return "!!";
		}

		@Override
		public int getRowCount() {
			return numRows;
		}

		@Override
		synchronized public Object getValueAt(int rowIndex, int columnIndex) {
			AbstractRecord rec = (AbstractRecord) call_name_idx.get(rowIndex);
			if (rec == null)
				return "";
			int cidx = columns.get(columnIndex);
			switch (cidx) {
			case COL_RUNNING_ICON:
				return rec.isRunning() ? runningIcon : normalIcon;
			case COL_NODENAME:
				return rec.getTargetName();
			case COL_NUM_NODES:
				return rec.numRecords();
			case COL_NUM_CALLS:
				return rec.getCallCount();
			}
			if(cidx >= COL_RECORDS) {
				int ridx = (cidx - COL_RECORDS) / 2;
				boolean is_all = (cidx - COL_RECORDS) % 2 == 0;
				if(is_all)
					return rec.getAllValue(ridx);
				else
					return rec.getSelfValue(ridx);
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

		public void callInsertEvent(AbstractRecord rec) {
			Integer idx = call_name_idx_rev.get(rec);
			TableModelEvent e;
			assert (idx != null);
			e = new TableModelEvent(this, idx, idx,
					TableModelEvent.ALL_COLUMNS, TableModelEvent.INSERT);

			callChangedEvent(e);
		}

		public void callChangedEvent(AbstractRecord rec) {
			Integer idx = call_name_idx_rev.get(rec);
			TableModelEvent e;
			if (idx == null)
				e = new TableModelEvent(this);
			else
				e = new TableModelEvent(this, idx);

			callChangedEvent(e);
		}

		synchronized public void callChangedEvent(TableModelEvent e) {
			for (TableModelListener l : listeners) {
				l.tableChanged(e);
			}
		}

		public void updateColumn() {
			callChangedEvent(new TableModelEvent(this,
					TableModelEvent.HEADER_ROW));
		}

	}

	int numRows;

	Map<Integer, AbstractRecord> call_name_idx;
	Map<AbstractRecord, Integer> call_name_idx_rev;
	MethodTableModel model;
	DataStore dataStore;
	

	RecordListView(DataStore dataStore) {
		super();
		this.dataStore = dataStore;
		
		columns = new ArrayList<Integer>();
		call_name_idx = new HashMap<Integer, AbstractRecord>();
		call_name_idx_rev = new HashMap<AbstractRecord, Integer>();
		model = new MethodTableModel();

		setModel(model);
		setAutoCreateRowSorter(true);
		numRows = 0;
		updateMode = false;
	}

	public void setDataStore(DataStore ds) {
		dataStore = ds;
		updateColumn();
	}
	public void updateColumn() {
		model.updateColumn();
	}

	public int getRowCount() {
		return numRows;
	}

	boolean updateMode;
	HashSet<AbstractRecord> updatingRecords;

	public void setUpdateMode(boolean stop) {
		if(stop) {
			updateMode = false;
			if(updatingRecords == null)
			{
				updatingRecords = new HashSet<AbstractRecord>();
			}
		}
		else {
			updateMode = true;
			synchronized (model) {
				for(AbstractRecord rec: updatingRecords) {
					model.callChangedEvent(rec);
				}
			}
		}
	}

	public void addRecord(AbstractRecord rec) {
		synchronized (model) {
			assert (!call_name_idx_rev.containsKey(rec));
			call_name_idx.put(call_name_idx.size(), rec);
			call_name_idx_rev.put(rec, call_name_idx_rev.size());
			assert (call_name_idx.size() == call_name_idx_rev.size());
			numRows++;
			model.callInsertEvent(rec);
		}
	}

	public void recordChanged(AbstractRecord rec) {
		if(!updateMode)
			updatingRecords.add(rec);
		else
		{
			synchronized (model) {
				model.callChangedEvent(rec);
			}
		}
	}

	public void valueChanged(ListSelectionEvent e) {
		// if(e.getValueIsAdjusting()) return;

		// DataStore.RecordSet recset =
		// (DataStore.RecordSet)call_name_idx.get(e.getFirstIndex());

	}

	public AbstractRecord getRecordAt(Point pt) {
		int row = convertRowIndexToModel(rowAtPoint(pt));
		return call_name_idx.get(row);

	}

}
