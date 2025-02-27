# 将wrf中原始的Fortran片段中的数组变量及其索引转换为Past格式（类C）
# 例如：将mkrdxm(i,j,k)转换为mkrdxm[i][j][k]
import re

expression = '''
// do n=1,nblocks_clinic
//   do j=2,ny_block
//     prevULAT_jm=ULAT(1,j-1,n)
//     zs=cos(prevULAT_jm)
//     prevULAT_j=ULAT(1,j,n)
//     zc=cos(prevULAT_j)
//     prevULON_jm=ULON(1,j-1,n)
//     prevULON_j=ULON(1,j,n)
//     prevYsum=(zs*sin(prevULON_jm))+
//              (zc*sin(prevULON_j))+
//              sin(prevULAT_j)
//     prevZsum=sin(prevULAT_jm)+
//              (zs*cos(prevULON_jm))+
//              (zc*cos(prevULON_j))
//     prevXsum=(zs*cos(prevULON_jm))+
//              (zc*cos(prevULON_j))
    
//     do i=2,nx_block
//       ULAT_jm=ULAT(i,j-1,n)
//       zs=cos(ULAT_jm)
//       ULON_jm=ULON(i,j-1,n)
//       ULAT_j=ULAT(i,j,n)
//       zc=cos(ULAT_j)
//       ULON_j=ULON(i,j,n)
//       xsum=(zs*cos(ULON_jm))+
//            (zc*cos(ULON_j))
//       ysum=(zs*sin(ULON_jm))+
//            (zc*sin(ULON_j))+
//            sin(ULAT_j)
//       zsum=sin(ULAT_jm)+sin(ULAT_j)
//       tx=25*(xsum+prevXsum)
//       ty=25*(ysum+prevYsum)
//       tz=25*(zsum+prevZsum)
//       ...
//       prevYsum=ysum; prevZsum=zsum;
//       prevXsum=xsum
//     end do
//   end do
// end do
'''

def convert_fortran_to_c(code):
    # Regular expression to find array references
    array_regex = re.compile(r'(\w+)\(([^)]+)\)')
    
    # Function to replace the Fortran indices with C indices
    def replace_indices(match):
        array_name = match.group(1)
        indices = match.group(2).split(',')
        
        # Reverse the order of indices
        reversed_indices = ']['.join(reversed([index.strip() for index in indices]))
        
        # Convert to C-style array reference
        return f"{array_name}[{reversed_indices}]"
    
    # Substitute all occurrences in the code
    converted_code = array_regex.sub(replace_indices, code)
    
    return converted_code

def convert_c_to_fortran(code):
    # Regular expression to match multi-dimensional arrays
    pattern_3d = re.compile(r'(\w+)\[([^\]]+)\]\[([^\]]+)\]\[([^\]]+)\]')
    pattern_2d = re.compile(r'(\w+)\[([^\]]+)\]\[([^\]]+)\]')
    pattern_1d = re.compile(r'(\w+)\[([^\]]+)\]')

    # Replace 3D array indices
    code = pattern_3d.sub(r'\1(\4,\2,\3)', code)
    # Replace 2D array indices
    code = pattern_2d.sub(r'\1(\3,\2)', code)

    code = pattern_1d.sub(r'\1(\2)', code)

    return code

c_expr = convert_fortran_to_c(expression)

dim_pattern = r'\b(i|k|j)\b'  # 匹配单词边界上的 i、k 或 j
dim_replacement = {
    'i': 'i',
    'k': 'k',
    'j': 'j'
}

dim_result = re.sub(dim_pattern, lambda match: dim_replacement[match.group()], c_expr)
result = re.sub(r'\s*&', '', dim_result)

for line in result.split('\n'):
    print(line + ';')