define print_last_five
  # 获取链表头指针
  set $head = kmem.freelist
  set $len = 0
  set $p = $head

  # --- 第一次遍历：计算链表总长度 ---
  while $p
    set $len = $len + 1
    set $p = $p->next
  end

  # --- 打印链表总长度 ---
  printf "Total free pages: %d\n", $len

  # --- 处理长度不足 5 的情况 ---
  if $len <= 5
    printf "All nodes (<=5):\n"
    set $p = $head
    set $i = 1
    while $p
      printf "  [%d] addr = %p, next = %p\n", $i, $p, $p->next
      set $p = $p->next
      set $i = $i + 1
    end
  else
    # --- 长度 ≥5 时，定位到倒数第5个节点 ---
    set $target_pos = $len - 5
    set $current_pos = 0
    set $p = $head

    # 移动指针到倒数第5个节点
    while $current_pos < $target_pos
      set $p = $p->next
      set $current_pos = $current_pos + 1
    end

    # --- 打印最后5个节点 ---
    printf "Last 5 free pages:\n"
    set $i = $target_pos + 1
    while $p
      printf "  [%d] addr = %p, next = %p\n", $i, $p, $p->next
      set $p = $p->next
      set $i = $i + 1
      if $i > $len  # 防止意外无限循环
        break
      end
    end
  end
end
