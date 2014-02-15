import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.net.URL;
import java.io.*;
import java.util.*;
import name.pachler.nio.file.*;

public class WKNetworkUI extends JPanel implements TreeSelectionListener, ActionListener {
    private DirectoryWatcher directorywatcher;
    private NetworkServer networkServer;

    // UI
    private JPanel sensorPanel;
    private JTextField sensorTextField;
    private JTree tree;
    private DefaultTreeModel treemodel;

    //Optionally play with line styles.  Possible values are
    //"Angled" (the default), "Horizontal", and "None".
    private static boolean playWithLineStyle = false;
    private static String lineStyle = "Horizontal";
    
    //Optionally set the look and feel.
    private static boolean useSystemLookAndFeel = true;

	private String networkdir = "/Users/niels/git/darjeeling/src/config/native-dollhouse/dollhouse/";

    private SensorTreeNode selectedSensorTreeNode = null;

    public WKNetworkUI() {
        super(new GridLayout(1,0));

        // Watch node directories for changes
        directorywatcher = new DirectoryWatcher(networkdir);

        //Create the nodes.
        DefaultMutableTreeNode top = new DefaultMutableTreeNode(networkdir);
        treemodel = new DefaultTreeModel(top);
        createNodes(top);

        //Create a tree that allows one selection at a time.
        tree = new JTree(treemodel);
        tree.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
		for (int i = 0; i < tree.getRowCount(); i++) {
		    tree.expandRow(i);
		}

        //Set icons for sensors and actuators
        tree.setCellRenderer(new DefaultTreeCellRenderer() {
            private Icon sensorIcon = UIManager.getIcon("InternalFrame.maximizeIcon");
            private Icon actuatorIcon = UIManager.getIcon("InternalFrame.closeIcon");
            private Icon connectedDeviceTreeNodeIcon = UIManager.getIcon("InternalFrame.maximizeIcon");
            private Icon disconnectedDeviceTreeNodeIcon = UIManager.getIcon("InternalFrame.closeIcon");
            @Override
            public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected, boolean expanded, boolean isLeaf, int row, boolean focused) {
                Component c = super.getTreeCellRendererComponent(tree, value, selected, expanded, isLeaf, row, focused);
                System.err.println(sensorIcon);
                if (value instanceof SensorTreeNode)
                    setIcon(sensorIcon);
                else if (value instanceof ActuatorTreeNode)
                    setIcon(actuatorIcon);
                else if (value instanceof DeviceTreeNode) {
                    int clientId = ((DeviceTreeNode)value).getClientId();
                    if (networkServer != null && networkServer.getConnectedClients().contains(clientId))
                        setIcon(connectedDeviceTreeNodeIcon);
                    else
                        setIcon(disconnectedDeviceTreeNodeIcon);
                }
                return c;
            }
        });

        //Listen for when the selection changes.
        tree.addTreeSelectionListener(this);
        if (playWithLineStyle) {
            System.out.println("line style = " + lineStyle);
            tree.putClientProperty("JTree.lineStyle", lineStyle);
        }

        //Create the scroll pane and add the tree to it. 
        JScrollPane treeView = new JScrollPane(tree);

        //Create the HTML viewing pane.
        sensorPanel = new JPanel();
		sensorTextField = new JTextField("", 20);
		sensorTextField.addActionListener(this);
		sensorTextField.setEnabled(false);
		sensorPanel.add(sensorTextField);
        JScrollPane sensorView = new JScrollPane(sensorPanel);

        //Add the scroll panes to a split pane.
        JSplitPane devicesPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT);
        devicesPane.setTopComponent(treeView);
        devicesPane.setBottomComponent(sensorView);

        Dimension minimumSize = new Dimension(100, 50);
        sensorView.setMinimumSize(minimumSize);
        treeView.setMinimumSize(minimumSize);

        JSplitPane mainPane = new JSplitPane(JSplitPane.VERTICAL_SPLIT);
        mainPane.setTopComponent(devicesPane);
        TextArea textArea = new TextArea();
        mainPane.setBottomComponent(textArea);

        // Start the network server
        networkServer = new NetworkServer();
        networkServer.setMessagesListener(new UIMessagesListener(textArea, tree, treemodel));
        networkServer.start();

        //Add the split pane to this panel.
        add(mainPane);
    }

    /** Required by TreeSelectionListener interface. */
    public void valueChanged(TreeSelectionEvent e) {
        DefaultMutableTreeNode node = (DefaultMutableTreeNode)
                           tree.getLastSelectedPathComponent();

        if (node instanceof SensorTreeNode) {
        	this.setSelectedSensorTreeNode((SensorTreeNode)node);
        } else {
        	this.setSelectedSensorTreeNode(null);
        }
    }
    private void setSelectedSensorTreeNode(SensorTreeNode s) {
        if (s == null) {
            sensorTextField.setText("");
            sensorTextField.setEnabled(false);
        } else {
            sensorTextField.setText(s.getValue().toString());
            sensorTextField.setEnabled(true);
        }
        this.selectedSensorTreeNode = s;
    }

    /** Required by ActionListener interface */
    public void actionPerformed(ActionEvent e) {
        if (e.getSource() == sensorTextField) {
            String text = sensorTextField.getText();
            try {
                int newval = Integer.parseInt(sensorTextField.getText());
                selectedSensorTreeNode.setValue(newval);
            } catch (NumberFormatException ex) {
            }
        }
    }

    private void createNodes(DefaultMutableTreeNode top) {
		File folder = new File(this.networkdir);
		File[] listOfFiles = folder.listFiles(); 
		for (int i = 0; i < listOfFiles.length; i++) {
			if (listOfFiles[i].isDirectory() && listOfFiles[i].getName().startsWith("node_")) {
				String dirname = listOfFiles[i].getName();
                try {
    				DeviceTreeNode node = new DeviceTreeNode(this.networkdir, dirname, this.directorywatcher, this.treemodel);
    			    top.add(node);
                } catch (IOException e) {
                    System.err.println("Error reading " + dirname);                    
                    System.err.println(e);
                }
			}
		}
    }
        
    /**
     * Create the GUI and show it.  For thread safety,
     * this method should be invoked from the
     * event dispatch thread.
     */
    private static void createAndShowGUI() {
        if (useSystemLookAndFeel) {
            try {
                UIManager.setLookAndFeel(
                    UIManager.getSystemLookAndFeelClassName());
            } catch (Exception e) {
                System.err.println("Couldn't use system look and feel.");
            }
        }

        //Create and set up the window.
        JFrame frame = new JFrame("WKNetworkUI");
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);

        //Add content to the window.
        frame.add(new WKNetworkUI());

        //Display the window.
        frame.pack();
        frame.setVisible(true);
    }

    public static void main(String[] args) {
        //Schedule a job for the event dispatch thread:
        //creating and showing this application's GUI.
        javax.swing.SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                createAndShowGUI();
            }
        });
    }
}