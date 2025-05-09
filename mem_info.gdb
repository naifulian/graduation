# 虚拟地址 -> 物理地址
define v2p
    # 获取传入的虚拟地址参数
    set $va = $arg0
    
    # 计算页目录索引(PDX) - 取虚拟地址的高 10 位
    set $pdx = ($va >> 22) & 0x3ff
    
    # 计算页表索引(PTX) - 取虚拟地址中间的 10 位
    set $ptx = ($va >> 12) & 0x3ff
    
    # 计算页内偏移(OFFSET) - 取虚拟地址的 低 10 位
    set $off = ($va  & 0xfff)
    
    # 打印准备阶段的信息
    printf "Prepare: VA=0x%08x PDX=0x%08x PTX = 0x%08x OFFSET = 0x%08x\n", $va, $pdx, $ptx, $off

    # 阶段1：查找页目录项(PDE)
    # 计算页目录项的地址 = CR3寄存器值(页目录基址) + PDX索引*4
    set $addr1 = $cr3 + ($pdx << 2)
    
    # 读取页目录项(PDE)内容，加上0x80000000是因为xv6内核映射在这个地址
    set $pde = *(pte_t*)($addr1 + 0x80000000)
    
    # 打印阶段1的信息
    printf "Stage1: cr3=0x%08x PDX=0x%08x ADDR1=0x%08x PDE = 0x%08x\n", $cr3, $pdx, $addr1, $pde

    # 阶段2：查找页表项(PTE)
    # 从PDE中提取页表物理页号(PPN)，清除低12位标志位
    set $ppn = $pde & ~0xfff
    
    # 计算页表项的地址 = 页表基址(PPN) + PTX索引*4
    set $addr2 = $ppn + ($ptx << 2)
    
    # 读取页表项(PTE)内容，同样加上0x80000000的内核映射偏移
    set $pte = *(pte_t*)($addr2 + 0x80000000)
    
    # 打印阶段2的信息
    printf "Stage2: PPN=0x%08x PTX=0x%08x ADDR2=0x%08x PTE = 0x%08x\n", $ppn, $ptx, $addr2, $pte

    # 最终阶段：计算物理地址
    # 从PTE中提取物理页号(PPN2)，清除低12位标志位
    set $ppn2 = $pte & ~0xfff
    
    # 组合物理页号和页内偏移得到完整物理地址
    set $pa = $ppn2 | $off
    
    # 打印最终转换信息
    printf "Final: PPN2=0x%08x OFFSET=0x%08x PA = 0x%08x\n", $ppn2, $off, $pa 
    
    # 打印总结信息
    printf "Summary: VA =0x%08x -> PA=0x%08x\n", $va, $pa
end

# 查看空闲链表的信息
define freemem
    # 获取空闲链表头指针，xv6中kmem.freelist指向第一个空闲页
    set $p = kmem.freelist
    
    # 初始化计数器
    set $len = 0
    
    # 遍历空闲链表
    while $p
        # 只显示前9个节点的信息
        if $len < 9
            printf "%d\t: %p\n", 1 + $len, $p
            # 注释掉的代码可以用于显示节点的具体内容
            #p/x *($p)
        end
        
        # 移动到下一个节点
        set $p = $p->next
        
        # 增加计数器
        set $len = $len + 1
    end 
    
    # 打印空闲页总数
    printf "total = %d\n", $len
end
