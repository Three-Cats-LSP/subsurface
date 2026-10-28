#!/usr/bin/env python3
import argparse,os,sys,json
from pathlib import Path
parser=argparse.ArgumentParser(description="Exercise the real shared workspace panel with a large synthetic version review; no cloud requests.")
parser.add_argument("--pyside-path",type=Path)
parser.add_argument("--output",type=Path,required=True)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
os.environ['QT_QPA_PLATFORM']='offscreen'
if args.pyside_path:sys.path.insert(0,str(args.pyside_path.resolve()))
os.chdir(Path(__file__).resolve().parents[1])
from PySide6.QtCore import QObject,Property,Slot,QUrl
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
class Cloud(QObject):
 calls=[]
 @Property(bool,constant=True)
 def syncInProgress(self): return False
 @Property(bool,constant=True)
 def authorizationInProgress(self): return False
 @Property(bool,constant=True)
 def accountCheckInProgress(self): return False
 @Property(str,constant=True)
 def workspaceProviderId(self): return ''
 @Slot(str,str,str)
 def runWorkspaceVersion(self,*args): self.calls.append(args)
app=QGuiApplication([]);engine=QQmlApplicationEngine();cloud=Cloud();engine.rootContext().setContextProperty('CloudSync',cloud)
uri=Path('mobile-widgets/qml/modern/components').resolve().as_uri()
qml='import QtQuick\nimport QtQuick.Controls\nimport "COMPONENTS" as Neo\nApplicationWindow {\n id: rootItem; width: 980; height: 800; visible: false\n property bool neoModernStyle: false\n property QtObject subsurfaceTheme: QtObject { property string currentTheme: "Light" }\n Neo.NeoWorkspacePanel { id: panel; objectName: "panel"; width: 900\n  provider: ({id: "google-drive", workspace: {versionReview: "review-a", localProtected: true, versionRows: []}})\n }\n function rows(count, review) {\n  var list=[]; for(var i=count;i>0;--i) list.push({id:"ticket-"+i,number:i,bytes:123,head:i===count,restored:false})\n  panel.provider={id:"google-drive",workspace:{versionReview:review,localProtected:true,versionRows:list}}\n }\n function find(item,name) { if(item.objectName===name)return item;var list=item.children||[];for(var i=0;i<list.length;i++){var found=find(list[i],name);if(found)return found}return null }\n function page() { var ids=[];function scan(item){if(String(item.objectName).indexOf("workspaceVersionRow-google-drive-")===0)ids.push(Number(String(item.objectName).split("-").pop()));var c=item.children||[];for(var i=0;i<c.length;i++)scan(c[i])}scan(panel);return JSON.stringify(ids.sort(function(a,b){return b-a})) }\n function older() { find(panel,"olderWorkspaceVersions-google-drive").clicked() }\n function newer() { find(panel,"newerWorkspaceVersions-google-drive").clicked() }\n function restoreOldest() { find(panel,"restoreWorkspaceVersion-google-drive-1").clicked() }\n function cancelRestore() { find(rootItem.contentItem,"cancelWorkspaceRestore-google-drive").clicked() }\n function pending() { return JSON.stringify(panel.pendingRestore) }\n}'
qml=qml.replace('COMPONENTS',uri);engine.loadData(qml.encode(),QUrl.fromLocalFile(str((args.output/'harness.qml').resolve())))
if not engine.rootObjects():raise RuntimeError('QML harness failed')
root=engine.rootObjects()[0]
def pump():
 for _ in range(5):app.processEvents()
root.rows(605,'review-a');pump();seen=[];pages=[]
for i in range(31):
 page=json.loads(root.page());assert len(page)<=20;seen+=page;pages.append(page)
 if i<30:root.older();pump()
assert seen==list(range(605,0,-1)),len(seen)
root.restoreOldest();pump();assert json.loads(root.pending())=={'source':'ticket-1','number':1,'review':'review-a'}
root.cancelRestore();pump();assert not cloud.calls
root.newer();pump();assert json.loads(root.page())==list(range(25,5,-1))
root.rows(605,'review-b');pump();assert json.loads(root.page())==list(range(605,585,-1))
result={'source':'actual NeoWorkspacePanel.qml','rows':605,'pages':31,'maxVisibleRows':20,'allVersionsReachable':True,'oldestRestoreReviewRetained':True,'cancelSendsNoAction':True,'newReviewResetsPage':True}
(args.output/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
