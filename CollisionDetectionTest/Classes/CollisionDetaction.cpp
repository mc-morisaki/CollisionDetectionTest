//
//  CollisionDetaction.cpp
//
//  Created by gear1554 on 2012/10/18.
//

#include "CollisionDetaction.h"
#include "cocos2d.h"

USING_NS_CC;

CollisionDetaction::CollisionDetaction(CollisionDetactionDelegate* _collisionDetectionDelegate,
                                       CCArray* _gameObjectArray,
                                       unsigned int spaceLevel){
    
    collisionDetectionDelegate = _collisionDetectionDelegate;
    gameObjectArray = _gameObjectArray;
    spaceArray = CCArray::create();
    spaceArray->retain();
    
    uiLevel = dwCellNum = 0;
    fW = fH = fLeft = fTop = fUnit_W = fUnit_H = 0.0f;
    checkHitCount =checkTreeSpaceCount = 0;
    
    int i;
    iPow[0] = 1;
    for(i=1;i<CLINER4TREEMANAGER_MAXLEVEL+1;i++){
        iPow[i] = iPow[i-1]*4;
        CCLOG("level%d spaceCount:%d", i, iPow[i]);
    }
    
    // Set collision detecsion space rect
    CCSize winSize = CCDirector::sharedDirector()->getWinSize();
    spaceRect = CCRect(0, 0, winSize.width, winSize.height);
    
    Init(spaceLevel, spaceRect.origin.x , spaceRect.size.height, spaceRect.size.width, spaceRect.origin.y);
    
}


CollisionDetaction::~CollisionDetaction(){
    spaceArray->removeAllObjects();
    spaceArray->release();
    hitObjectList.clear();
}


bool CollisionDetaction::Init(int Level, float left, float top, float right, float bottom){
    
    if(Level>=CLINER4TREEMANAGER_MAXLEVEL)
        return false;
    
    dwCellNum = (iPow[Level+1]-1)/3;
    CCLOG("CollisionDetaction spaceSize:%d", dwCellNum);
    for(register int i = 0; i<dwCellNum; i++){
        CCArray *spaceCellArray = CCArray::create();
        spaceArray->addObject(spaceCellArray);
    }
    
    fLeft = left;
    fTop = top;
    fW = right - left;
    fH = bottom - top;
    fUnit_W = (float)(fW/(1<<Level));
    fUnit_H = (float)(fH/(1<<Level));
    
    uiLevel = Level;
    
    return true;
}


int CollisionDetaction::getMortonNumberFromNode(CCNode* node){
    
    float left      = node->getPosition().x - node->getContentSize().width  /2;
    float top       = node->getPosition().y + node->getContentSize().height /2;
    float right     = node->getPosition().x + node->getContentSize().width  /2;
    float bottom    = node->getPosition().y - node->getContentSize().height /2;
    
    int LT = GetPointElem(left, top);
    int RB = GetPointElem(right, bottom);
    
    int Def = RB ^ LT;
    unsigned int HiLevel = 0;
    unsigned int i;
    for(i=0; i<uiLevel; i++)
    {
        int Check = (Def>>(i*2)) & 0x3;
        if( Check != 0 )
            HiLevel = i+1;
    }
    int SpaceNum = RB>>(HiLevel*2);
    int AddNum = (iPow[uiLevel-HiLevel]-1)/3;
    SpaceNum += AddNum;
    
    if(SpaceNum > dwCellNum){
        return 0xffffffff;
    }
    
    return SpaceNum;
}

int CollisionDetaction::getMortonNumberFromPosition(CCPoint position){
    float left      = position.x - 1;
    float top       = position.y + 1;
    float right     = position.x + 1;
    float bottom    = position.y - 1;
    
    int LT = GetPointElem(left, top);
    int RB = GetPointElem(right, bottom);
    
    int Def = RB ^ LT;
    unsigned int HiLevel = 0;
    unsigned int i;
    for(i=0; i<uiLevel; i++)
    {
        int Check = (Def>>(i*2)) & 0x3;
        if( Check != 0 )
            HiLevel = i+1;
    }
    int mortonNumber = RB >> ( HiLevel * 2 );
    int AddNum = ( iPow[ uiLevel - HiLevel ] - 1 ) / 3;
    mortonNumber += AddNum;
    
    if(mortonNumber > dwCellNum)
        return 0xffffffff;
    
    return mortonNumber;
}


int CollisionDetaction::BitSeparate32(int n){
    n = (n|(n<<8)) & 0x00ff00ff;
    n = (n|(n<<4)) & 0x0f0f0f0f;
    n = (n|(n<<2)) & 0x33333333;
    return (n|(n<<1)) & 0x55555555;
}


int CollisionDetaction::Get2DMortonNumber(int x, int y){
    return (int)(BitSeparate32(x) | BitSeparate32(y) << 1);
}


int CollisionDetaction::GetPointElem(float pos_x, float pos_y){
    return Get2DMortonNumber((int)((pos_x - fLeft) / fUnit_W), (int)((pos_y - fTop) / fUnit_H));
}


// Update Collision detection
void CollisionDetaction::update(){
    
    // Refresh space status
    refreshSpace();
    
    if(!gameObjectArray || gameObjectArray->count() < 2){
        return;
    }
    
    // Update game objects's space status
    CCObject *obj;
    CCARRAY_FOREACH(gameObjectArray, obj){
        updateSpaceStatus((CCSprite*)obj);
    }
    
    // Scan collision detection
    scanCollisionDetection(0, CCArray::createWithCapacity(0));
    
    // Collision Handling
    for(vector<CollisionPair>::iterator itr = hitObjectList.begin(); itr != hitObjectList.end(); ++itr){
        collisionDetectionDelegate->onCollide(*itr);
    }
}


// Update
void CollisionDetaction::updateSpaceStatus(CCNode *collisionObject){
    
    int spaceIndex = getMortonNumberFromNode(collisionObject);
    
    if(spaceIndex < 0 || spaceIndex >= dwCellNum ){
        return;
    }
    
    CCArray *spaceCellArray = (CCArray*)spaceArray->objectAtIndex(spaceIndex);
    
    if(!spaceCellArray->containsObject(collisionObject)){
        spaceCellArray->addObject(collisionObject);
    }
}


bool CollisionDetaction::isHavingChildSpace(int spaceIndex){
    checkTreeSpaceCount++;
    return (spaceIndex >= 0 && spaceIndex < dwCellNum) ? true :false;
}


void CollisionDetaction::refreshSpace(){
    
    CCObject *obj;
    CCARRAY_FOREACH_REVERSE(spaceArray, obj){
        CCArray *spaceCellArray = (CCArray*)obj;
        spaceCellArray->removeAllObjects();
    }
    
    hitObjectList.clear();
    checkHitCount = 0;
    checkTreeSpaceCount = 0;
}


void CollisionDetaction::scanCollisionDetection(int spaceIndex, CCArray* stackArray){
    CCArray *spaceCellArray;
    
    if(!spaceArray->retainCount() || !spaceArray->count()){
        return;
    }
    spaceCellArray = (CCArray*)spaceArray->objectAtIndex(spaceIndex);
    
    checkHitSpaceCell(spaceCellArray, spaceCellArray);
    checkHitSpaceCell(spaceCellArray, stackArray);
    
    bool childFlag = false;
    int ObjNum = 0;
    int nextSpaceIndex;
    for(register int i = 0; i<4; i++){
        nextSpaceIndex = spaceIndex * 4 + 1 + i;
        if(isHavingChildSpace(nextSpaceIndex)){
            if(!childFlag){
                CCObject* obj;
                CCARRAY_FOREACH(spaceCellArray, obj){
                    stackArray->addObject((CCNode*)obj);
                }
                ObjNum += spaceCellArray->count();
            }
            
            childFlag = true;
            scanCollisionDetection(nextSpaceIndex, stackArray);
        }
    }
    
    if( childFlag ){
        for(int i = 0; i<ObjNum; i++){
            stackArray->removeLastObject();
        }
    }
    
    return;
}


void CollisionDetaction::checkHit(CCNode *collisionObject1, CCNode *collisionObject2){
    
    if(collisionDetectionDelegate->detectCollision(collisionObject1, collisionObject2)){
        hitObjectList.push_back(CollisionPair(collisionObject1, collisionObject2));
    }
    checkHitCount++;
}

void CollisionDetaction::checkHitSpaceCell(CCArray* array1, CCArray* array2){
    
    if(array1 == array2){
        for(register int i = 0; i<array1->count(); i++){
            for(register int j = i + 1; j<array2->count(); j++){
                if(j < array2->count()){
                    checkHit((CCNode*)array1->objectAtIndex(i), (CCNode*)array2->objectAtIndex(j));
                }
            }
        }
    }else{
        CCObject *obj;
        CCARRAY_FOREACH(array1, obj){
            CCObject *obj2;
            CCARRAY_FOREACH(array2, obj2){
                if(obj != obj2){
                    checkHit((CCNode*)obj, (CCNode*)obj2);
                }
            }
        }
    }
}

int CollisionDetaction::getSpaceNumBySpaceLevel(unsigned int spaceLevel){
    return iPow[spaceLevel];
}
