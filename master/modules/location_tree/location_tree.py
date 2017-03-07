class loc_tree(tornado.web.RequestHandler):
  def post(self):
    global node_infos

    addloc = template.Loader(os.getcwd()).load('templates/display_locationTree2.html').generate(node_infos=node_infos)
    wkpf.globals.location_tree.printTree()
    disploc = wkpf.globals.location_tree.getJson()

    self.content_type = 'application/json'
    self.write({'loc':json.dumps(disploc),'node':addloc})

  def get(self, node_id):
    global node_infos
    node_id = int(node_id)
    curNode = wkpf.globals.location_tree.findLocationById(node_id)
    print node_id, curNode
    if curNode == None:
        self.write({'status':1,'message':'cannot find node id '+str(node_id)})
        return
    else:
        print curNode.distanceModifier
        self.write({'status':0, 'message':'succeed in finding node id'+str(node_id),
                    'distanceModifierByName':curNode.distanceModifierToString(), 'distanceModifierById':curNode.distanceModifierIdToString(),
                    'centerPnt':curNode.centerPnt,
                    'size':curNode.size, 'location':curNode.getLocationStr(), 'local_coord':curNode.getOriginalPnt(),
                    'global_coord':curNode.getGlobalOrigPnt(), 'landmarks': json.dumps(curNode.getLandmarkList())})

  def put(self, node_id):
    global node_infos
    node_id = int(node_id)
    curNode = wkpf.globals.location_tree.findLocationById(node_id)
    if curNode == None:
        self.write({'status':1,'message':'cannot find node id '+str(node_id)})
        return
    else:
        global_coord = self.get_argument("global_coord")
        local_coord = self.get_argument("local_coord")
        size = self.get_argument("size")
        direction = self.get_argument("direction")
        curNode.setLGSDFromStr(local_coord, global_coord, size, direction)
        self.write({'status':0,'message':'find node id '+str(node_id)})
class loc_tree_parse_policy(tornado.web.RequestHandler):
    def get(self):
        policy = self.get_argument("policy")
        ret = wkpf.globals.location_parser.parse(policy)
        self.write({'status':1,'message':str(ret) })
        return

class edit_loc_tree(tornado.web.RequestHandler):
    def post(self):
        operation = self.get_argument("operation")
        parent_id = self.get_argument("parent_id")
        child_name = self.get_argument("child_name")
        size = self.get_argument("size")
        paNode = wkpf.globals.location_tree.findLocationById(int(parent_id))
        if paNode != None:
            if operation == "0": #add a new location
                paNode.addChild(child_name)
                print ("add child", child_name)
                self.write({'status':0,'message':'successfully add child '+child_name })
                return
            elif operation == "1": #delete a location
                childNode = paNode.findChildByName(child_name)
                if childNode != None:
                    paNode.delChild(childNode)
                    self.write({'status':0,'message':'successfully delete child '+child_name })
                    return
                else:
                    self.write({'status':1,'message': child_name +'not found' })
                    return
        else:
            self.write({'status':1,'message':'parentNode does not exist in location tree :('})
            return
class sensor_info(tornado.web.RequestHandler):
    def get(self, node_id, sensor_id):
        global node_infos
        node_id = int(node_id)
        curNode = wkpf.globals.location_tree.findLocationById(node_id)
        if curNode == None:
            self.write({'status':1,'message':'cannot find node id '+str(node_id)})
            return
        if sensor_id[0:2] =='se':   #sensor case
            se_id = int(sensor_id[2:])
            sensr = curNode.getSensorById(se_id)
            self.write({'status':0,'message':'find sensor id '+str(se_id), 'location':sensr.location})
        elif sensor_id[0:2] =='lm': #landmark case
            lm_id = int(sensor_id[2:])
            landmk = curNode.findLandmarkById(lm_id)
            self.write({'status':0,'message':'find landmark id '+str(lm_id), 'location':landmk.location,'size':landmk.size, 'direction':landmk.direction})
        else:
            self.write({'status':1, 'message':'failed in finding '+sensor_id+" in node"+ str(node_id)})

class tree_modifier(tornado.web.RequestHandler):
  def put(self, mode):
    start_id = self.get_argument("start")
    end_id = self.get_argument("end")
    distance = self.get_argument("distance")
    paNode = wkpf.globals.location_tree.findLocationById(int(start_id)//100)      #find parent node
    if paNode !=None:
        if int(mode) == 0:        #adding modifier between siblings
            if paNode.addDistanceModifier(int(start_id), int(end_id), int(distance)):
                self.write({'status':0,'message':'adding distance modifier between '+str(start_id) +'and'+str(end_id)+'to node'+str(int(start_id)//100)})
                return
            else:
                self.write({'status':1,'message':'adding failed due to not able to find common direct father of the two nodes'})
                return
        elif int(mode) == 1:        #deleting modifier between siblings
            if paNode.delDistanceModifier(int(start_id), int(end_id), int(distance)):
                self.write({'status':0,'message':'deletinging distance modifier between '+str(start_id) +'and'+str(end_id)+'to node'+str(int(start_id)//100)})
                return
            else:
                self.write({'status':1,'message':'deleting faild due to not able to find common direct father of the two nodes'})
                return
    self.write({'status':1,'message':'operation faild due to not able to find common direct father of the two nodes'})


class save_landmark(tornado.web.RequestHandler):
  def put(self):

        self.write({'tree':wkpf.globals.location_tree})

  def post(self):
        wkpf.globals.location_tree.saveTree()
        self.write({'message':'Save Successfully!'})

class load_landmark(tornado.web.RequestHandler):
    def post(self):
        flag = os.path.exists("../LocalData/landmarks.txt")
        if(flag):
            wkpf.globals.location_tree.loadTree()
            self.write({'message':'Load Successfully!'})
        else:
            self.write({'message':'"../LocalData/landmarks.txt" does not exist '})

class add_landmark(tornado.web.RequestHandler):
  def put(self):
    global landId

    name = self.get_argument("name")
    id = 0;
    location = self.get_argument("location")
    operation = self.get_argument("ope")
    size  = self.get_argument("size")
    direct = self.get_argument("direction")
    coordinate = self.get_argument("coordinate")
    landmark = None
    rt_val = 0
    msg = ''
    if(operation=="1"):
      landId += 1
      landmark = LandmarkNode(name, location+"@"+coordinate, size, direct)
      rt_val = wkpf.globals.location_tree.addLandmark(landmark)
      msg = 'add fails'
      wkpf.globals.location_tree.printTree()
    elif(operation=="0"):
      rt_val = wkpf.globals.location_tree.delLandmark(name, location)
      msg = 'deletion of '+ name + ' fails at '+ location
    self.content_type = 'application/json'
    if rt_val == True:
        self.write({'status':0, 'id':name, 'msg':'change succeeds'})
    if rt_val == False:
        self.write({'status':1, 'id':name, 'msg':msg})

