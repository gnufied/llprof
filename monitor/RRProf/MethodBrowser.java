package RRProf;

import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallNameRecordSet;
import RRProf.DataStore.ThreadStore;

public class MethodBrowser extends RecordListView implements
		DataStore.RecordEventListener, DataStore.EventListener, MouseListener {

	Monitor mon;
	MonitorBrowser main_gui;

	MethodBrowser(MonitorBrowser mgui) {
		super(null);
		main_gui = mgui;

		addMouseListener(this);
	}

	public void close() {
		if(mon != null && mon.getDataStore() != null)
			mon.getDataStore().removeEventListener(this);
		removeMouseListener(this);
		mon = null;
		
	}

	public void setMonitor(Monitor m) {
		mon = m;
		mon.getDataStore().addEventListener(this);
		setDataStore(mon.getDataStore());
		columns.clear();
		columns.add(COL_RUNNING_ICON);
		columns.add(COL_NODENAME);
		columns.add(COL_RECORD_ALL(0));
		columns.add(COL_RECORD_SELF(0));
		columns.add(COL_RECORD_ALL(1));
		columns.add(COL_RECORD_SELF(1));
		columns.add(COL_NUM_CALLS);
		columns.add(COL_NUM_NODES);
		updateColumn();
	}

	@Override
	public void addCallName(CallNameRecordSet rset) {
		rset.addRecordEventListener(this);
		addRecord(rset);
	}

	@Override
	public void addRecordChild(AbstractRecord rec) {
	}

	@Override
	public void updateParentRecordData(AbstractRecord rec) {
		recordChanged(rec);
	}

	@Override
	public void updateRecordData(AbstractRecord rec) {
		recordChanged(rec);
	}

	@Override
	public void addRecordMember(AbstractRecord rec) {

	}

	@Override
	public void mouseClicked(MouseEvent e) {
		if (e.getClickCount() == 2) {
			DataStore.RecordSet record = (DataStore.RecordSet) getRecordAt(e
					.getPoint());
			if (record != null) {
				if (e.getButton() == e.BUTTON1) {
					main_gui.openRecordSetView(record.childrenRecord());
				}
				if (e.getButton() == e.BUTTON3) {
					main_gui.openRecordSetView(record);
				}
			}
		}
	}

	@Override
	public void mouseEntered(MouseEvent e) {
	}

	@Override
	public void mouseExited(MouseEvent e) {
	}

	@Override
	public void mousePressed(MouseEvent e) {
	}

	@Override
	public void mouseReleased(MouseEvent e) {
	}

	@Override
	public void addThread(ThreadStore threadStore) {

	}

}
