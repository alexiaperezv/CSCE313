#include <iostream>
#include <vector> // Added this in step 1 (fixing syntax)
using namespace std; //Added this in step 1 (fixing syntax)


class node {
public: // added this in step 1 (fixing syntax)
    int val;
    node* next;
};
 
void create_LL(vector<node*>& mylist, int node_num){
    mylist.assign(node_num, NULL);

    //create a set of nodes
    for (int i = 0; i < node_num; i++) {
        //Blank C
        mylist[i] = new node(); // Added this (first run time error)
        mylist[i]->val = i;
        mylist[i]->next = NULL;
    }

    //create a linked list
    for (int i = 0; i < node_num-1; i++) { // changed the boundary to node_num-1 (second run time error)
            mylist[i]->next = mylist[i+1];
    }
}

int sum_LL(node* ptr) {
    int ret = 0;
    while(ptr) {
        ret += ptr->val;
        ptr = ptr->next;
    }
    return ret;
}

int main(int argc, char ** argv){
    const int NODE_NUM = 3;
    vector<node*> mylist;

    create_LL(mylist, NODE_NUM);
    int ret = sum_LL(mylist[0]); 
    cout << "The sum of nodes in LL is " << ret << endl;

    //Step 4: delete nodes, everything below this line was added to deallocate memory 
    node* prev; 
    node* current = mylist[0];
    while(current != NULL)
    {
        prev = current;
        current = current->next;
        delete prev;
    }
    delete current;

}