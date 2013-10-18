using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace FlvPlayer
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void ButtonPlayClick(object sender, RoutedEventArgs e)
        {
            Player.Source = new Uri(@"fttp://file/?u=E:\SOURCESAFE\flv\samples\92785010.flv", UriKind.Absolute);
            //Player.Source = new Uri(@"fttp://61.55.172.171/h1/flvs/CD0C5D3C8614B28B/2013-09-18/B0F832ED630F91039C33DC5901307461-1.flv?t=1381840892&key=5FC21A4C99FA2C5123156AD351FD7A57&upid=4243461381833692037");
            // Player.Source = new Uri(@"G:\Videos\k301.mp4");
  //          Player.Play();
        }
    }
}
