package RRProf;



import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;

import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallNameRecordSet;
import RRProf.DataStore.ThreadStore;

public class MethodBrowser extends RecordListView implements DataStore.RecordEventListener, DataStore.EventListener, MouseListener{

	Monitor mon;
	MonitorGUI main_gui;
	
	public MethodBrowser(MonitorGUI mgui)
	{
		super();
		main_gui = mgui;
		columns.add(this.COL_RUNNING_ICON);
		columns.add(this.COL_CLASS);
		columns.add(this.COL_METHOD);
		columns.add(this.COL_ALL_TIME);
		columns.add(this.COL_SELF_TIME);
		columns.add(this.COL_NUM_CALLS);
		columns.add(this.COL_NUM_NODES);
		updateColumn();
		addMouseListener(this);
	}
	
	
	public void setMonitor(Monitor m)
	{
		mon = m;
		mon.getDataStore().addEventListener(this);
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
	    if(e.getClickCount() == 2) {
	    	DataStore.RecordSet record = (DataStore.RecordSet)getRecordAt(e.getPoint());
	    	if(record != null)
	    	{
	    		if(e.getButton() == e.BUTTON1)
	    		{
	    			main_gui.openRecordSetView(record.childrenRecord());
	    		}
	    		if(e.getButton() == e.BUTTON3)
	    		{
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
