.text 
    pushq %rbp              
    movq %rsp, %rbp         
    subq $32, %rsp         

    addl $30, %edi          
    movl %edi, -20(%rbp)    
    movl %edi, -16(%rbp)    
    movl %edi, -12(%rbp)    
    movl %edi, -8(%rbp)    
    movl %edi, -4(%rbp)    

    movl %edi, %eax


    imul $5, %eax 

    movl $100, %eax    
    imul $15, %eax 

    movl %eax, -20(%rbp)    
    movl %eax, -16(%rbp)    
    movl %eax, -12(%rbp)    
    movl %eax, -8(%rbp)    
    movl %eax, -4(%rbp)    

    movl -20(%rbp), %eax
    movl -16(%rbp), %eax

    movl %edi, %eax    

    leave 
    ret


    movl $10, %edi
    call x

    imul %edi, %eax
    addl %edi, %eax
    subl %edi, %eax