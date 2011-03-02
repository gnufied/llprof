package RRProf;

import java.awt.BorderLayout;

import javax.swing.*;

import RRProf.DataStore.AbstractRecord;
import RRProf.DataStore.CallNameRecordSet;

public class RecordSetViewer extends JPanel {
	JSplitPane sp;
	JTextArea field;

	
	public RecordSetViewer(DataStore.RecordSet recset){
		RecordSetBrowser list = new RecordSetBrowser();
		list.setRecordSet(recset);
        JScrollPane list_scroll = new JScrollPane();
        list_scroll.getViewport().setView(list);
		field = new JTextArea();
		sp = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
       sp.setDividerSize(5);
       sp.setLeftComponent(list_scroll);
       sp.setRightComponent(field);
       
       setLayout(new BorderLayout());
       add(sp, BorderLayout.CENTER);

       field.setText("aaa");
       
	}
}
