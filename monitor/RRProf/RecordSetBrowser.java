package RRProf;

import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallNameRecordSet;
import RRProf.DataStore.RecordSet;
import RRProf.DataStore.ThreadStore;

public class RecordSetBrowser extends RecordListView implements DataStore.RecordEventListener{
	
	public RecordSetBrowser(DataStore ds)
	{
		super(ds);
		columns.add(COL_RUNNING_ICON);
		columns.add(COL_NODENAME);
		columns.add(COL_RECORD_ALL(0));
		columns.add(COL_NUM_NODES);
		updateColumn();
	}
	
	RecordSet recordSet;
	public void setRecordSet(RecordSet rset)
	{
		recordSet = rset;
		recordSet.addRecordEventListener(this);
	
		for(AbstractRecord record: recordSet)
		{
			addRecord(record);
		}
		assert(recordSet.numRecords() == this.getRowCount());
	}



	@Override
	public void addRecordChild(AbstractRecord rec) {
	}


	@Override
	public void updateParentRecordData(AbstractRecord rec) {
	}


	@Override
	public void updateRecordData(AbstractRecord rec) {
		recordChanged(rec);
		assert recordSet.numRecords() == this.getRowCount();
	}

	@Override
	public void addRecordMember(AbstractRecord rec) {
		addRecord(rec);
		assert(recordSet.numRecords() == this.getRowCount());
	}

}


