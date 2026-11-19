#include <iostream>
#include <bitset>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <array>
#include <variant>
#include <random>
#include <list>
#include <map>         
#include <functional> 
#include <cmath>      
#include <numeric>     
#include <ctime>       
#include <chrono>  
#include <set>
#include <process.h>

// windows-specific for memory usage
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

using namespace std;

// helper to query peak memory usage (bytes)
static size_t getPeakMemoryUsageBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.PeakWorkingSetSize);
    }
#endif
    return 0;
}

// Add a single file-scope RNG seeded once (used by crossoverRandom)
namespace {
    static std::mt19937 rng{ static_cast<unsigned int>(time(nullptr)) };
}


// string file = "SDB.txt";

// // online GOOD
// string file = "dataset/Online.txt";
// const int MAXL = 270;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 4383;  // maximum number of sequences
// const int MAXE = 9;    // maximum number of items in an element
// const int C = 90000;  // total columns in bitset matrix

// // simple
// string file = "dataset/simple.txt";
// const int MAXL = 3;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 4;  // maximum number of sequences
// const int MAXE = 4;    // maximum number of items in an element
// const int C = 12;  // total columns in bitset matrix

// // BIKE 
// string file = "dataset/BIKE.txt";
// const int MAXL = 53;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 21078;  // maximum number of sequences
// const int MAXE = 1;    // maximum number of items in an element
// const int C = 160000;  // total columns in bitset matrix

// // ESHOP 
// string file = "dataset/ESHOP_8x.txt";
// const int MAXL = 100;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 24026*8;  // maximum number of sequences
// const int MAXE = 20;    // maximum number of items in an element
// const int C = 170000*8;  // total columns in bitset matrix

// string file = "dataset/100-8-8-10k-6.txt";
// const int MAXL = 8;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 10000;  // maximum number of sequences
// const int MAXE = 8;    // maximum number of items in an element
// const int C = 80000;  // total columns in bitset matrix


// string file = "dataset/FIFA.txt";
// const int MAXL = 100;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 20450;  // maximum number of sequences
// const int MAXE = 1;    // maximum number of items in an element
// const int C = 750000;  // total columns in bitset matrix

// string file = "dataset/Leviathan.txt";
// const int MAXL = 100;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 5834;  // maximum number of sequences
// const int MAXE = 1;    // maximum number of items in an element
// const int C = 200000;  // total columns in bitset matrix

string file = "dataset/SIGN.txt";
const int MAXL = 100;    // maximum length of a sequence (number of items in a sequence)
const int MAXS = 730;  // maximum number of sequences
const int MAXE = 1;    // maximum number of items in an element
const int C = 70000;  // total columns in bitset matrix

// string file = "dataset/Bible.txt";
// const int MAXL = 100;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 36369;  // maximum number of sequences
// const int MAXE = 1;    // maximum number of items in an element
// const int C = 800000;  // total columns in bitset matrix

// string file = "dataset/BMS1.txt";
// const int MAXL = 10;    // maximum length of a sequence (number of items in a sequence)
// const int MAXS = 59601;  // maximum number of sequences
// const int MAXE = 1;    // maximum number of items in an element
// const int C = 160000;  // total columns in bitset matrix

const int max_generations = 8000;
// 衰退率与默认 TopK
const double decay_rate = 0.95;
const double mutation_rate = 0;
const int TopK = 500;  
const int early_stop_generations = 10;
const double tournament_percent = 0.3; // 锦标赛选择比例
int allneg = 0;

int findnextrow_combined(const vector<bitset<C>>& mat, const vector<int>& tags, const int* lengths);

class Database {
public: 
    unordered_map<int, bitset<C>> data;
    int each_L[MAXS] = {0};
    int seqCount = 0;

    // 修改为 array<int, MAXE> 作为键
    map<array<int, MAXE>, pair<int, unordered_set<int>>> seqStat1;
    unordered_map<int, pair<int, unordered_set<int>>> seqStat2;

    void loadFromFile(const std::string& filename) {
        std::ifstream fin(filename);
        if (!fin.is_open()) {
            cerr << "Cannot open file: " << filename << endl;
            return;
        }

        int sid = 0; // sequence id
        array<int, MAXE> elem1 = {0}; // 修改为 array<int, MAXE>
        int elem2 = 0;
        int elem_pos = 0; // 当前填充到 elem 的位置
        int pos = 0; // bitset位置
        string line;
        while (std::getline(fin, line)) {
            if (sid >= MAXS) {
                cerr << "Warning: Maximum sequence count reached (MAXS=" << MAXS 
                    << ") - remaining sequences will be ignored." << endl;
                break;
            }
            std::istringstream iss(line);
            int num;
            int cnt_L = 0;
            bool hasfound = false;
            while (iss >> num) {
                if (num == -2) {
                    each_L[sid] = cnt_L;
                    break;
                } else if (num == -1) {
                    // if (pos > MAXL - 1) {
                    //     cerr << "Warning: Position index out of bounds." << endl;
                    // }
                    pos++;
                    cnt_L++;
                    // 只处理非空 elem
                    if (elem_pos > 1) {
                        array<int, MAXE> key = elem1;
                        // 多余位置补 0
                        for (int i = elem_pos; i < MAXE; ++i) key[i] = 0;
                        if (auto it = seqStat1.find(key); it != seqStat1.end()) {
                            it->second.first++;    // 更新 pair 的第一个元素
                            it->second.second.insert(sid);   // 更新 pair 的第二个元素
                        } else {
                            seqStat1[key] = {1, {sid}};
                        }
                        elem1.fill(0); // 每行开始前清零
                        elem_pos = 0;
                    }
                    elem1.fill(0);
                    elem_pos = 0;
                } else {
                    if (elem_pos > MAXE - 1) {
                        cerr << "Warning: Element size exceeds MAXE." << endl;
                    }
                    if (elem_pos < MAXE) {
                        elem1[elem_pos++] = num;
                    }
                    seqStat2[num].first++;
                    seqStat2[num].second.insert(sid);
                    // ensure we don't write beyond MAXS (extra safety)
                    if (sid < MAXS) {
                        // data[num][sid].set(pos);
                        // 将sid和pos映射到bitset的线性索引
                        // int bitIdx = sid * MAXL + pos;
                        data[num].set(pos);
                    }
                }
            }
            ++sid; // next sequence
        }
        fin.close();
        seqCount = static_cast<int>(min(sid, MAXS));
    }

    void printDatabase() const {
        cout << "Database contents:\n";
        for (const auto &p : data) {
            int item = p.first;
            const auto &bits = p.second;
            cout << "Item " << item << ": ";
            int used = seqCount;
            for (int sid = 0; sid < used; ++sid) {
                cout << "[s" << sid << ":";
                for (int pos = 0; pos < MAXL; ++pos) {
                    int bitIdx = sid * MAXL + pos;
                    cout << bits[bitIdx];
                }
                cout << "] ";
            }
            cout << "\n";
        }
    }
};

class eliteMinHeap {
private:
    std::vector<int> heap;
    long long total_sum = 0; // Variable to track the current total sum
    int maxsup = 0;

    // Heapify a subtree rooted at index i
    void heapify(int i) {
        int smallest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < heap.size() && heap[left] < heap[smallest]) {
            smallest = left;
        }

        if (right < heap.size() && heap[right] < heap[smallest]) {
            smallest = right;
        }

        if (smallest != i) {
            std::swap(heap[i], heap[smallest]);
            heapify(smallest);
        }
    }

public:
    // Insert a new value into the heap
    void insert(int val) {
        if (maxsup < val) maxsup = val;
        heap.push_back(val);
        total_sum += val; // Update sum
        int i = heap.size() - 1;

        // Bubble up to maintain heap property
        while (i > 0 && heap[(i - 1) / 2] > heap[i]) {
            std::swap(heap[(i - 1) / 2], heap[i]);
            i = (i - 1) / 2;
        }
    }

    // Extract and return the minimum value
    int extractMin() {
        if (heap.empty()) {
            throw std::runtime_error("Heap is empty");
        }

        int minVal = heap[0];
        total_sum -= minVal; // Update sum
        heap[0] = heap.back();
        heap.pop_back();

        if (!heap.empty()) {
            heapify(0);
        }

        return minVal;
    }

    // Get the minimum value without extracting
    int getMin() const {
        if (heap.empty()) {
            throw std::runtime_error("Heap is empty");
        }
        return heap[0];
    }

    // Get the current total sum
    long long getSum() const {
        return total_sum;
    }

    pair<int, int> getRange() const{
        if (heap.empty()) {
            throw std::runtime_error("Heap is empty");
        }
        int minVal = heap[0];
        return {minVal, maxsup};
    }

    // Check if heap is empty
    bool isEmpty() const {
        return heap.empty();
    }

    // Get the size of the heap
    int size() const {
        return heap.size();
    }
};

struct Gene {
    bool is_neg = false;
    bool is_single = true;

    using itemSet = variant<int, array<int, MAXE>>;
    itemSet itemset;

    Gene() : itemset(0) {} // Default to an int value of 0

    void setItem(int val) {
        is_single = true;
        itemset = val;
    }

    void setItem(const array<int, MAXE>& vals) {
        is_single = false;
        itemset = vals;
    }

    itemSet getItem() const {
        return itemset;
    }

    void printItem() const {
        if (is_single) {
            cout << "Value: " << get<int>(itemset);
        } else {
            cout << "Values: ";
            for (const int& val : get<array<int, MAXE>>(itemset)) {
                if (val != 0){
                    cout << val << " ";
                }
            }
        }
    }
};

// 全局：存储初始化阶段产生的所有基因
vector<pair<Gene, int>> posGenes;
vector<pair<Gene, int>> negGenes;
eliteMinHeap eliteHeap;

struct Individual {
    vector<Gene> genes;
    int fitness = 0;
    int support = 0;
    int dfitness = 0;
    int uid = 0; // 新增：全局唯一 id，用于索引和 O(1) 删除

    void addGene(const Gene& gene) {
        genes.push_back(gene);
    }

    void mutation() {
        // 轮盘赌选择一个负基因
        int total = 0;
        for (const auto& p : negGenes) total += p.second;
        if (total == 0) return;
        std::uniform_int_distribution<int> roulette(1, total);
        int r = roulette(rng);
        int acc = 0;
        int chosenIdx = 0;
        for (; chosenIdx < negGenes.size(); ++chosenIdx) {
            acc += negGenes[chosenIdx].second;
            if (r <= acc) break;
        }

        // std::uniform_int_distribution<int> typeDist(0, 1);
        // int mutationType = typeDist(rng);

        // if (mutationType == 0) {
            // 随机选择一个负基因，轮盘赌变为另一个负基因
            vector<int> negIdxs;
            for (int i = 0; i < genes.size(); ++i) {
                if (genes[i].is_neg) negIdxs.push_back(i);
            }
            if (!negIdxs.empty()) {
                std::uniform_int_distribution<int> idxDist(0, negIdxs.size() - 1);
                int targetIdx = negIdxs[idxDist(rng)];
                genes[targetIdx] = negGenes[chosenIdx].first;
            }
        // } else {
        // }
    }

    void neg_explore(){
        // 轮盘赌选择一个负基因
        int total = 0;
        for (const auto& p : negGenes) total += p.second;
        if (total == 0) return;
        std::uniform_int_distribution<int> roulette(1, total);
        int r = roulette(rng);
        int acc = 0;
        int chosenIdx = 0;
        for (; chosenIdx < negGenes.size(); ++chosenIdx) {
            acc += negGenes[chosenIdx].second;
            if (r <= acc) break;
        }

        //  在允许添加负基因的位置随机插入一个负基因
        vector<int> insertable;
        for (int i = 0; i <= genes.size(); ++i) {
            // 不能在负基因两边插入负基因
            bool leftNeg = (i > 0) && genes[i - 1].is_neg;
            bool rightNeg = (i < genes.size()) && genes[i].is_neg;
            if (!leftNeg && !rightNeg) insertable.push_back(i);
        }
        if (!insertable.empty()) {
            std::uniform_int_distribution<int> posDist(0, insertable.size() - 1);
            int pos = insertable[posDist(rng)];
            genes.insert(genes.begin() + pos, negGenes[chosenIdx].first);
        }
    }

    void repair() {
        // 检查是否有负基因
        bool hasNeg = false;
        for (const auto& g : genes) {
            if (g.is_neg) {
                hasNeg = true;
                break;
            }
        }
        if (!hasNeg) {        
            // 轮盘赌选择一个负基因
            int total = 0;
            for (const auto& p : negGenes) total += p.second;
            if (total == 0) return;
            std::uniform_int_distribution<int> dist(1, total);
            int r = dist(rng);
            int acc = 0;
            int idx = 0;
            for (; idx < negGenes.size(); ++idx) {
                acc += negGenes[idx].second;
                if (r <= acc) break;
            }
            const Gene& chosenNeg = negGenes[idx].first;

            // 随机插入到任意位置
            std::uniform_int_distribution<int> posDist(0, genes.size());
            int pos = posDist(rng);
            genes.insert(genes.begin() + pos, chosenNeg);
        } else {
            // 已有负基因
            // 检查是否有连续两个负基因
            for (int i = 1; i < genes.size(); ++i) {
                if (genes[i-1].is_neg && genes[i].is_neg) {
                    // 轮盘赌选择一个正基因
                    int total = 0;
                    for (const auto& p : posGenes) total += p.second;
                    if (total == 0) break;
                    std::uniform_int_distribution<int> dist(1, total);
                    int r = dist(rng);
                    int acc = 0;
                    int idx = 0;
                    for (; idx < posGenes.size(); ++idx) {
                        acc += posGenes[idx].second;
                        if (r <= acc) break;
                    }
                    const Gene& chosenPos = posGenes[idx].first;

                    // // 随机插入到这两个负基因之间的一个位置
                    // std::uniform_int_distribution<int> posDist(0, 1);
                    // int insertPos = posDist(rng) ? i : i-1;
                    genes.insert(genes.begin() + i, chosenPos);
                    break; // 只处理一次
                }
            }
        }
    }


    int calsup(const Database& db) {
        int m = genes.size();
        vector<bitset<C>> matrix_bit;
        vector<int> tags;
        for(const auto& gene : genes){
            bitset<C> row;
            if(gene.is_single){
                int item = get<int>(gene.itemset);
                if(db.data.find(item) == db.data.end()){
                    cerr << "Warning: Item " << item << " not found in database." << endl;
                    return -1;
                }
                if(gene.is_neg){
                    row = ~db.data.at(item);
                    tags.push_back(1);
                } else {
                    row = db.data.at(item);
                    tags.push_back(0);
                }
            } else {
                const auto& items = get<array<int, MAXE>>(gene.itemset);
                row.set();
                if(gene.is_neg){
                    tags.push_back(1);
                } else {
                    tags.push_back(0);
                }
                for(const auto& item : items){
                    if(item != 0){
                        if(db.data.find(item) == db.data.end()){
                            cerr << "Warning: Item " << item << " not found in database." << endl;
                            return -1;
                        }
                        if(gene.is_neg){
                            row &= ~db.data.at(item);
                        } else {
                            row &= db.data.at(item);
                        }
                    }
                }
            }
            matrix_bit.push_back(row);
        }

        int sup = findnextrow_combined(matrix_bit, tags, db.each_L);
        support = sup;
        return sup;
    }

    void printIndividual() const {
        // cout << "Individual with " << genes.size() << " genes:\n";
        for (int i = 0; i < genes.size(); ++i) {
            if (genes[i].is_neg) cout << "NEG ";
            genes[i].printItem();
            if ( genes.size() != 1 && i != genes.size() - 1) cout << " -> ";
        }
    }
};

class Population {
public:
    // 主存储：链表，便于 O(1) splice/move
    list<Individual> individuals;
    
    // 签名 -> 链表中节点的 iterator（便于 O(1) 查找）
    unordered_map<string, list<Individual>::iterator> signatureIndex;
    // 用于记录仅作登记的衍生个体签名（这些个体不加入种群，不建立 uid/index）
    unordered_set<string> derivedSignatures;

    // ---------- 新增索引用于 O(1) 随机选择 & 按 support 删除 ----------
    int nextUid = 1; // 用于分配 uid

    // 全部个体的数组（用于 O(1) 随机选取），存储 list<Individual>::iterator
    vector<list<Individual>::iterator> allVec;
    unordered_map<int, int> uidToAllIndex; // uid -> index in allVec

    // uid -> index in its supportBuckets[support] vector
    unordered_map<int, int> uidToBucketIdx;
    // -------------------------------------------------------------------

    // 生成个体签名（不变）
    string signatureOf(const Individual& ind) const {
        ostringstream oss;
        oss << ind.genes.size() << '|';
        for (const Gene& g : ind.genes) {
            oss << (g.is_neg ? '1' : '0') << (g.is_single ? 'S' : 'M') << ':';
            if (g.is_single) {
                oss << get<int>(g.itemset);
            } else {
                const auto& arr = get<array<int, MAXE>>(g.itemset);
                for (int i = 0; i < MAXE; ++i) {
                    if (i) oss << ',';
                    oss << arr[i];
                }
            }
            oss << ';';
        }
        return oss.str();
    }

private:
    // 内部帮助：把 iterator 插入到 allVec 与 supportBuckets，并建立索引（O(1)）
    void indexNewIterator(list<Individual>::iterator it) {
        int uid = nextUid++;
        it->uid = uid;

        // allVec
        allVec.push_back(it);
        uidToAllIndex[uid] = allVec.size() - 1;
    }

    // 内部帮助：从 allVec 中移除某个 iterator（O(1)）
    void removeFromAllVec(list<Individual>::iterator it) {
        int uid = it->uid;
        int idx = uidToAllIndex[uid];
        int last = allVec.size() - 1;
        if (idx != last) {
            auto swappedIt = allVec[last];
            allVec[idx] = swappedIt;
            uidToAllIndex[swappedIt->uid] = idx;
        }
        allVec.pop_back();
        uidToAllIndex.erase(uid);
    }

    void removeIndividualByIterator(list<Individual>::iterator it){
        // 删除该个体（O(1)）
        // removeFromSupportBucket(it, it->support);
        // cout << "Removing individual uid=" << it->uid << endl;
        removeFromAllVec(it);
        string sig = signatureOf(*it);
        auto sigIt = signatureIndex.find(sig);
        if (sigIt != signatureIndex.end() && sigIt->second == it) signatureIndex.erase(sigIt);
        individuals.erase(it);
    }

public:
    // 锦标赛选择：每次随机选3个个体，取支持度最高者，执行两次得到两个父代，并使这两个父代的动态适应度*decay_rate
    vector<Individual> select() {
        vector<Individual> ret;
        int selectCount = 2;
        int tournamentSize = allVec.size() * tournament_percent >= 3 ? allVec.size() * tournament_percent : 3; // 10% 作为锦标赛规模
        for (int t = 0; t < selectCount; ++t) {
            // if (allVec.size() < tournamentSize) break;
            vector<int> idxs;
            // 随机抽取符合要求的个体进入锦标赛
            while (idxs.size() < tournamentSize) {
                int idx = std::uniform_int_distribution<int>(0, allVec.size() - 1)(rng);
                // cout << "rng idx: " << idx << endl;
                auto it = allVec[idx];
                // 筛选：单正基因且support < min_sup则删除
                if ((it->genes.size() == 1 && !it->genes[0].is_neg && it->support < eliteHeap.getMin()) || it->dfitness < 0.1) {
                    removeIndividualByIterator(it);
                    continue; // 继续抽取
                }
                // if (it->support < (eliteHeap.getMin() > 3000 ? eliteHeap.getMin() : 3000)) continue; // 不符合要求，继续抽取
                idxs.push_back(idx);
            }
            list<Individual>::iterator bestIt = allVec[*idxs.begin()];
            for (auto idx : idxs) {
                auto it = allVec[idx];
                if (it->dfitness > bestIt->dfitness) bestIt = it;
            }
            bestIt->dfitness = static_cast<int>(bestIt->dfitness * decay_rate);
            ret.push_back(*bestIt);
        }
        return ret;
    }

public:
    bool addIndividual(const Individual& ind) {

        string sig = signatureOf(ind);
        if(signatureIndex.find(sig) == signatureIndex.end()){
            // 新个体，插入到链表尾
            individuals.push_back(ind);

            auto itNew = prev(individuals.end());
            // 分配 uid 并建立索引
            indexNewIterator(itNew);

            if (ind.genes.size() == 1 && !ind.genes[0].is_neg) return false;

            // 保证 dfitness 与 support 同步（初始化阶段的约定）
            itNew->dfitness = itNew->support;
            // 记录签名指向该迭代器
            signatureIndex.emplace(sig, itNew);

            if(eliteHeap.size() < TopK){
                eliteHeap.insert(itNew->support);
                ind.printIndividual();
                cout << " support=" << itNew->support << "\n";
            }
            else if(eliteHeap.size() == TopK && itNew->support > eliteHeap.getMin()){
                eliteHeap.extractMin();
                eliteHeap.insert(itNew->support);
                ind.printIndividual();
                cout << " support=" << itNew->support << "\n";
            }

            // 新策略：对于该个体中每一个多项基因（非单项），移除该多项中的每一个元素
            // （每移除一个元素就生成一个新个体），将该新个体的 support 加入 eliteHeap，
            // 且不改变该新个体的 fitness/support/dfitness
            bool isnegpresent = false;
            for (const auto& g : ind.genes) {
                if (g.is_neg) {
                    isnegpresent = true;
                    break;
                }
            }
            if (!isnegpresent || ind.genes.size() == 1) return false;
            Gene g;
            Individual derived;
            for (int gi = 0; gi < ind.genes.size(); ++gi) {
                g = ind.genes[gi];
                if (g.is_single) continue; // 只处理多项基因
                auto arr = get<array<int, MAXE>>(g.itemset);
                for (int pos = 0; pos < MAXE; ++pos) {
                    if (arr[pos] == 0) continue; // 跳过空位
                    // 生成一个派生个体：移除该位置的元素并压缩其余元素
                    derived = ind;
                    array<int, MAXE> newArr{};
                    int idx = 0;
                    for (int k = 0; k < MAXE; ++k) {
                        if (k == pos) continue; // 移除此元素
                        if (arr[k] != 0) newArr[idx++] = arr[k];
                    }

                    // 如果压缩后只剩一个元素，则设为单项基因
                    int nonzeroCnt = 0;
                    int lastVal = 0;
                    for (int k = 0; k < MAXE; ++k) {
                        if (newArr[k] != 0) { nonzeroCnt++; lastVal = newArr[k]; }
                    }
                    if (nonzeroCnt == 1) {
                        Gene sg;
                        sg.setItem(lastVal);
                        sg.is_neg = g.is_neg;
                        derived.genes[gi] = sg;
                    } else {
                        derived.genes[gi].setItem(newArr);
                        derived.genes[gi].is_neg = g.is_neg;
                    }

                    // 不改变 support/dfitness（derived 是从 ind 复制而来）
                    // 先检查签名以避免重复登记：如果既不在正式签名索引也不在衍生签名集合中，则登记
                    string dsig = signatureOf(derived);
                    if (signatureIndex.find(dsig) == signatureIndex.end() && derivedSignatures.find(dsig) == derivedSignatures.end()) {
                        allneg++;
                        // 只作登记（不加入种群、不建立 uid），避免参与选择和繁衍
                        derivedSignatures.insert(dsig);

                        // 将该派生个体的 support 插入 eliteHeap（同原逻辑）
                        if (eliteHeap.size() < TopK) {
                            eliteHeap.insert(derived.support);
                            // derived.printIndividual();
                            // cout << " support=" << derived.support << "\n";
                        } else if (derived.support > eliteHeap.getMin()) {
                            eliteHeap.extractMin();
                            eliteHeap.insert(derived.support);
                            // derived.printIndividual();
                            // cout << " support=" << derived.support << "\n";
                        }
                    }
                }
            }
            return true;
        } else return false;
    }

    void Initialize(const Database& db) {
        for (const auto& kv : db.seqStat2) {
            int item = kv.first;
            const auto& val = kv.second;

            // 单正基因
            Individual posInd;
            Gene g;
            g.setItem(item);
            g.is_neg = false;
            posInd.addGene(g);
            posInd.support = val.first;
            posInd.dfitness = val.first;
            posGenes.push_back(pair{g, val.first});
            addIndividual(posInd);

            // 单负基因
            Individual negInd;
            Gene ng;
            ng.setItem(item);
            ng.is_neg = true;
            negInd.addGene(ng);
            negInd.support = db.seqCount - static_cast<int>(val.second.size());
            negInd.dfitness = negInd.support;
            negGenes.push_back(pair{ng, negInd.support});
            addIndividual(negInd);
        }
        for (const auto& kv : db.seqStat1) {
            const array<int, MAXE>& key = kv.first;
            const auto& val = kv.second;

            // 单正基因
            Individual posInd;
            Gene g;
            g.setItem(key);
            g.is_neg = false;
            posInd.addGene(g);
            posInd.support = val.first;
            posInd.dfitness = val.first;
            posGenes.push_back(pair{g, val.first});
            addIndividual(posInd);

            // 单负基因
            Individual negInd;
            Gene ng;
            ng.setItem(key);
            ng.is_neg = true;
            negInd.addGene(ng);
            negInd.support = db.seqCount - static_cast<int>(val.second.size());
            negInd.dfitness = negInd.support;
            negGenes.push_back(pair{ng, negInd.support});
            addIndividual(negInd);
        }
    }

    int size() const { return individuals.size(); }

    void printAll() const {
        int idx = 0;
        for (const auto &ind : individuals) {
            cout << "Individual #" << idx++ << " support=" << ind.support << " difitness=" << ind.dfitness << "\n";
            ind.printIndividual();
        }
    }


    // 随机选择两个切点并交叉，参数为个体本身
    pair<Individual,Individual> crossover(const Individual& a, const Individual& b) {
        int na = static_cast<int>(a.genes.size());
        int nb = static_cast<int>(b.genes.size());
        // if (na == 0 || nb == 0) return nullopt;
        std::uniform_int_distribution<int> distA(0, na);
        std::uniform_int_distribution<int> distB(0, nb);

        int ca = distA(rng);
        int cb = distB(rng);

        bool a_is_excel = a.support >= eliteHeap.getMin() || (na == 1 && a.genes[0].is_neg);
        bool b_is_excel = b.support >= eliteHeap.getMin() || (nb == 1 && b.genes[0].is_neg);

        Individual c1, c2;
        auto isEdgeA = (ca == 0 || ca == na);
        auto isEdgeB = (cb == 0 || cb == nb);
        
        if (!isEdgeA || !isEdgeB){
            // 交叉
            for (int i = 0; i < ca; ++i) c1.addGene(a.genes[i]);
            for (int i = cb; i < nb; ++i) c1.addGene(b.genes[i]);
            for (int i = 0; i < cb; ++i) c2.addGene(b.genes[i]);
            for (int i = ca; i < na; ++i) c2.addGene(a.genes[i]);
        }
        else if (a_is_excel && b_is_excel) {
            // 拼接
            for (const auto& g : a.genes) c1.addGene(g);
            for (const auto& g : b.genes) c1.addGene(g);
            for (const auto& g : b.genes) c2.addGene(g);
            for (const auto& g : a.genes) c2.addGene(g);
        } else {
            // 传递
            c1.genes = a.genes;
            c2.genes = b.genes;
        }
        
        return pair{c1, c2};
    }  

};

// 在长矩阵上计算所有子矩阵的路径数
int findnextrow_combined(const vector<bitset<C>>& mat, const vector<int>& tags, const int* lengths) {
    int total_count = 0;
    int R = mat.size();
    int pos = 0;
    for (int seg = 0; seg < MAXS; ++seg) {
        int len = lengths[seg];
        int r = 0, c = -1;
        int count = 0;
        int entrance = -1;

        while (r >= 0 && r < R && c < len) {
            // 最后一行
            if (r == R - 1) {
                entrance = c;
                if (tags[r] == 1) {
                    while (c < len - 1) {
                        if (!mat[r][pos + c + 1]) {
                            r--;
                            break;
                        }
                        c++;
                    }
                    if (c == len - 1) count++;
                    if (entrance == len - 1) c++;
                    else {
                        c = entrance;
                        r--;
                    }
                } else {
                    bool findpath = false;
                    while (c < len - 1) {
                        if (mat[r][pos + c + 1]) {
                            findpath = true;
                            count++;
                            // 可以从当前位置开始计算的情况（1，2）即有1也有2：
                            //模式<1,2>可以结束于（1.2）也可以开始于（1，2）
                            if (tags[0] && mat[0][pos + c + 1]) r = 1;
                            else r = 0;
                            c++;
                            break;
                        }
                        c++;
                    }
                    if (!findpath) break;
                }
                continue;
            }
            // tag == 0
            if (tags[r] == 0) {
                bool findpath = false;
                while (c < len - 1) {
                    if (mat[r][pos + c + 1]) {
                        r++;
                        c++;
                        findpath = true;
                        break;
                    }
                    c++;
                }
                if (!findpath) break;
                continue;
            }
            // tag == 1
            if (tags[r] == 1) {
                entrance = c;
                if (c == len - 1) break;
                if (mat[r + 1][pos + c + 1]) {
                    r++;
                    continue;
                }
                if(c == -1 || mat[r][pos + c]) c++;
                // c++;
                while (c < len - 1) {
                    if (mat[r + 1][pos + c + 1] && mat[r][pos + c]) {
                        r++;
                        break;
                    } else if (!mat[r][pos + c]) {
                        c = entrance;
                        r--;
                        break;
                    } else c++;
                }
            }
        }
        total_count += count;
        pos += len;
    }
    return total_count;
}

void GA(const Database& db) {
    if (db.seqCount == 0) {
        cerr << "Database is empty. Exiting GA." << endl;
        return;
    }
    // 初始化种群
    Population pop;
    pop.Initialize(db);

    int generation = 0;
    int cnt = 0;
    long long prevSum = eliteHeap.getSum();
    while (1) {
        auto parents = pop.select();
        auto [child1, child2] = pop.crossover(parents[0], parents[1]);

        // 变异与修复
        child1.repair();
        child2.repair();

        // 随机变异：每个子代独立概率
        std::uniform_real_distribution<double> mrand(0.0, 1.0);
        if (mrand(rng) < mutation_rate) {
            child1.mutation();
        }
        if (mrand(rng) < mutation_rate) {
            child2.mutation();
        }

        child1.neg_explore();
        child2.neg_explore();

        // 计算支持度
        child1.calsup(db);
        child2.calsup(db);

        pop.addIndividual(child1);
        pop.addIndividual(child2);

        if (eliteHeap.getSum() - prevSum > 0) {
            parents[0].dfitness /= decay_rate;
            parents[1].dfitness /= decay_rate;
            prevSum = eliteHeap.getSum();
            cnt = 0;
        } else cnt++;

        // if (cnt >= early_stop_generations) {
        if (generation >= max_generations) {
        // if (eliteHeap.size() >= TopK &&eliteHeap.getRange().first >= 705){
            // 明确打印来源并保证函数返回以结束 GA 运行（避免只 break 出 while 后仍有后续阻塞）
            cout << " allneg=" << allneg << " No improvement for " << early_stop_generations << " generations, stopping early." << endl;
            pop.individuals.clear();
            pop.allVec.clear();
            pop.signatureIndex.clear();
            // pop.derivedSignatures.clear();
            return; // 直接返回，确保 GA 结束并回到 main
        }

        generation++;
        cout << "Generation " << generation << " population size: " << pop.size() 
            << "  Range: " << eliteHeap.getRange().first << " - " << eliteHeap.getRange().second 
            << " Heapsize: " << eliteHeap.size() << " Totalsup: " << eliteHeap.getSum() << endl;
    }
    return;
}


int main() {
    auto start = std::chrono::high_resolution_clock::now(); // 计时开始

    Database db;
    db.loadFromFile(file);

    GA(db);

    auto end = std::chrono::high_resolution_clock::now(); // 计时结束
    std::chrono::duration<double> elapsed = end - start;
    cout << "程序总耗时: " << elapsed.count() << " 秒" << endl;

    // report peak memory usage if available
    size_t peakBytes = getPeakMemoryUsageBytes();
    if (peakBytes > 0) {
        double peakMB = peakBytes / (1024.0 * 1024.0);
        cout << "峰值内存使用: " << peakMB << " MB (" << peakBytes << " bytes)" << endl;
    }

    return 0;
}