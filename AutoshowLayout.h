constexpr char kCarDetailsLayout[] = R"FH2XAML(<Page
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008" xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" xmlns:local="clr-namespace:ForzaUI" mc:Ignorable="d"
    x:Class="ForzaUI.A_CarDetails"
    x:Name="Page"
    WindowTitle="Page"
    Style="{StaticResource A_Style_Page_Generic}" 
    DataContext="{Binding RelativeSource={x:Static RelativeSource.Self}}" >
    
    <local:UIPageTransitionManager.TransitionManager>
        <local:UIPageTransitionManager DurationSpread="{StaticResource A_Duration_Transition_Spread}" TransitionDelay="{StaticResource A_Duration_Transition_Delay}"/>
    </local:UIPageTransitionManager.TransitionManager>

    <Page.Resources>
        <ResourceDictionary>
            <!--
            <ResourceDictionary.MergedDictionaries>
                <ResourceDictionary Source="/Resources/Anthem/Shared_Bars.xaml"/>
            </ResourceDictionary.MergedDictionaries>
            -->
            <Style x:Key="A_Style_FZTextBlock_Cost" TargetType="{x:Type local:FZTextBlock}" BasedOn="{StaticResource A_Style_FZTextBlock_AnthemA}" >
                <Setter Property="FontSize" Value="88"/>
            </Style>
            <Color x:Key="A_Color_Text_CreditsCost">#FFF5F5F5</Color>
            <Color x:Key="A_Color_Text_TokenCost">#FFFFFF4B</Color>
            <local:NumUnitConverter x:Key="A_NumUnitConverter_NoDecimal" ShowUnit="False" FractionDigits="0" />
        </ResourceDictionary>
    </Page.Resources>

    <Grid>
        <ContentControl x:Name="TopBar" Style="{StaticResource A_Style_PlayerBar_WithTokens}" />
        <ContentControl x:Name="BottomBar" Style="{StaticResource A_Style_ButtonBar}">
            <ContentControl x:Name="DefaultHelpPanel"
                    local:ControllerButtonPanel.ADescription="{local:Loc HelpButtons.IDS_Select}"
                    local:ControllerButtonPanel.BDescription="{local:Loc HelpButtons.IDS_Back}"
                    local:ControllerButtonPanel.XDescription="{local:Loc HelpButtons.IDS_ChangeCamera}"
                    local:ControllerButtonPanel.YDescription="{local:Loc HelpButtons.IDS_Explode}"
                    Style="{StaticResource A_Style_ContentControl_DefaultButtonBar}" />
        </ContentControl>
    
    <Border x:Name="LayoutRoot" Style="{StaticResource A_Style_Border_ScenePadding}">
        <VisualStateManager.VisualStateGroups>
            <VisualStateGroup x:Name="VisualStateGroup">
                <VisualState x:Name="IdleState" >
                    <Storyboard>
                        <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.Opacity)" Storyboard.TargetName="LayoutRoot">
                            <EasingDoubleKeyFrame KeyTime="0:0:0.5" Value="0"/>
                        </DoubleAnimationUsingKeyFrames>
                    </Storyboard>
                </VisualState>
                <VisualState x:Name="ActiveState">
                    <Storyboard>
                        <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.Opacity)" Storyboard.TargetName="LayoutRoot">
                            <EasingDoubleKeyFrame KeyTime="0:0:0.1" Value="1"/>
                        </DoubleAnimationUsingKeyFrames>
                    </Storyboard>
                </VisualState>
            </VisualStateGroup>
            <VisualStateGroup x:Name="CostStateGroup">
                <VisualState x:Name="ShowCosts" >
                    <Storyboard>
                        <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.Opacity)" Storyboard.TargetName="grid2">
                            <EasingDoubleKeyFrame KeyTime="0" Value="1"/>
                        </DoubleAnimationUsingKeyFrames>
                    </Storyboard>
                </VisualState>
                <VisualState x:Name="HideCosts">
                    <Storyboard>
                        <DoubleAnimationUsingKeyFrames Storyboard.TargetProperty="(UIElement.Opacity)" Storyboard.TargetName="grid2">
                            <EasingDoubleKeyFrame KeyTime="0" Value="0"/>
                        </DoubleAnimationUsingKeyFrames>                        
                    </Storyboard>
                </VisualState>
            </VisualStateGroup>
        </VisualStateManager.VisualStateGroups>

        <Grid>
            <!-- the credits and token cost grid -->            
            <Grid x:Name="grid1" Style="{StaticResource A_Style_FrameworkElement_FadeOnlyTransition_0}" Margin="-13,40,0,0">
            <Grid x:Name="grid2">
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="87"/>
                    <ColumnDefinition Width="600"/>
                </Grid.ColumnDefinitions>
                <Grid.RowDefinitions>
                    <RowDefinition Height="87"/>
                    <RowDefinition Height="87"/>
                    <RowDefinition/>
                    <RowDefinition Height="49*"/>
                </Grid.RowDefinitions>
                                <local:FZMaterial 
                    ImageSource="{StaticResource A_ImageSource_CreditIcon}" 
                    Margin="8.5" 
                    Width="70" 
                    Height="70" 
                    HorizontalAlignment="Center" 
                    VerticalAlignment="Center" 
                    Color="{DynamicResource A_Color_Text_CreditsCost}"
                    />
                <local:FZMaterial 
                    ImageSource="{StaticResource A_ImageSource_TokenIcon}" 
                    Grid.Row="1" 
                    Width="70" 
                    Height="70" 
                    HorizontalAlignment="Center" 
                    VerticalAlignment="Center" 
                    Color="{DynamicResource A_Color_Text_CreditsCost}"
					Visibility="{Binding AreTokensEnabled, Converter={StaticResource A_FZBooleanToVisibilityConverter}}" Margin="8.5"
                    />          
                <local:FZTextBlock
                    Grid.Column="1" 
                    Style="{DynamicResource A_Style_FZTextBlock_Cost}" 
                    HorizontalAlignment="Left" 
                    VerticalAlignment="Center" 
                    Color="{DynamicResource A_Color_Text_CreditsCost}" 
                    Margin="4,-11.637,0,78.363"
                    Text="{Binding TotalCreditCost, ConverterParameter=money, Converter={StaticResource A_NumUnitConverter_NoDecimal}}" Grid.RowSpan="2" 
                    />
                <local:FZTextBlock 
                    Grid.Column="1" 
                    Style="{DynamicResource A_Style_FZTextBlock_Cost}" 
                    HorizontalAlignment="Left" 
                    VerticalAlignment="Center" 
                    Color="{DynamicResource A_Color_Text_CreditsCost}" 
                    Margin="4,75.363,0,6.527" 
                    Text="{Binding TotalTokenCost, ConverterParameter=tokenmoney, Converter={StaticResource A_NumUnitConverter_NoDecimal}}"
					Visibility="{Binding AreTokensEnabled, Converter={StaticResource A_FZBooleanToVisibilityConverter}}" Grid.RowSpan="3"
                    />
                </Grid>
            </Grid>
        </Grid>
    </Border>
    </Grid>
</Page>)FH2XAML";
constexpr char kViewCarLayout[] = R"FH2XAML(<Page xmlns:local="clr-namespace:ForzaUI"
      xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
      xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
      xmlns:System="clr-namespace:System;assembly=mscorlib"
      x:Class="ForzaUI.A_ViewCar"
      mc:Ignorable="d"
      Style="{StaticResource A_Style_Page_Generic}"
      DataContext="{Binding RelativeSource={RelativeSource Self}}">

    <Page.Resources>
        <ResourceDictionary>
            <!--
            <ResourceDictionary.MergedDictionaries>
                <ResourceDictionary Source="/Resources/Anthem/Shared_Bars.xaml"/>
            </ResourceDictionary.MergedDictionaries>
            -->
        </ResourceDictionary>
    </Page.Resources>

    <Grid Style="{StaticResource A_Style_ScreenSize}">
        <local:FZMaterial Preset="{DynamicResource A_FZMaterialPresetScreenShadow_ARUI}"/>

        <ContentControl x:Name="BottomBar" Style="{StaticResource A_Style_ButtonBar}">
            <ContentControl x:Name="DefaultHelpPanel"
                    local:ControllerButtonPanel.BDescription="{local:Loc HelpButtons.IDS_Back}"
                    local:ControllerButtonPanel.XDescription="{local:Loc HelpButtons.IDS_ChangeCamera}"
                    local:ControllerButtonPanel.YDescription="{local:Loc HelpButtons.IDS_Explode}"
				    Style="{StaticResource A_Style_ContentControl_DefaultButtonBar}" />
        </ContentControl>
    </Grid>
</Page>)FH2XAML";
