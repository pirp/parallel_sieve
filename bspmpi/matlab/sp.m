clear all; close all;
for k=[8,9]
    figure(k);
    str = ['sp1e' int2str(k) '.txt'];
    f = dlmread(str);
    huy = f(:,[1 2]);

    huy_seq = huy(1,2);
    v = 1;
    for i=2:size(huy,1)
        r = huy_seq/huy(i,2);
        v = [v r];
    end
 
    plot(huy(1:6,1),v(1:6),'-*');

    hold on;
    plot(huy(1:6,1),huy(1:6,1),'r');
    legend('Huygens','Ideal');
    hold off;
    legend('Location','NorthWest');
    str3 = ['Speedup plot for n=10^8' int2str(k)];
    xlabel('p');
    ylabel('Speedup');
    set(k,'Position',[386 91 588 567]);
    str2 = ['sp1e' int2str(k)];
    print(k,'-depsc','-r300',str2);
    %clear all; close all;
end